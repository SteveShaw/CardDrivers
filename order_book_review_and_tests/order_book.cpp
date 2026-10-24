#include "order_book.hpp"
#include <order_book.hpp>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <order_book.hpp>
#include <fmt/format.h>
#include <iterator>
#include <utility>

namespace cpplib
{
void OrderBook::remove_order( Order* order )
{
    if( order->next )
    {
        order->next->prev = order->prev;
    }
    else
    {
        //this is the last order in the level
        order->ptr_level->tail = order->prev;
    }

    if( order->prev )
    {
        order->prev->next = order->next;
    }
    else
    {
        //this is the first order in the level
        order->ptr_level->head = order->next;
    }

    order->ptr_level->num_orders -= 1;
    
    if( order->ptr_level->num_orders == 0 )
    {
        switch( order->side )
        {
        case Side::Buy:
            bids.erase( order->ptr_level->level_pos );
            break;

        case Side::Sell:
            asks.erase( order->ptr_level->level_pos );
            break;

        default:
            break;
        }
    }

    order->ptr_level->total_qty -= order->qty;

    orders.erase( order->id );
}

void OrderBook::insert_order( uint64_t id, Side side, int64_t prc, int64_t qty )
{
    auto [it_order, succ] = orders.try_emplace( id );
    if( !succ )
    {
        return;
    }

    it_order->second = std::make_unique<Order>(id, side, prc, qty);
    auto ptr_order = it_order->second.get();

    switch( side )
    {
    case Side::Buy:
        ptr_order->ptr_level = insert_or_get_level( bids, prc, qty );
        break;

    case Side::Sell:
        ptr_order->ptr_level = insert_or_get_level( asks, prc, qty );
        break;
    }

    auto level = ptr_order->ptr_level;
    ptr_order->prev = level->tail;
    if( level->tail )
    {
        level->tail->next = ptr_order;
    }
    else
    {
        level->head = ptr_order;
    }

    level->tail = ptr_order;
    level->num_orders += 1;
    level->total_qty += qty;
}

Level* OrderBook::insert_or_get_level( level_map_t& level_map, int64_t prc, int64_t qty )
{
    auto [it, succ] = level_map.try_emplace( prc );
    if( succ )
    {
        it->second = std::make_unique<Level>();
    }

    it->second->level_pos = it;

    return it->second.get();
}

std::vector<Trade> OrderBook::match_buy_side( std::optional<int64_t> prc, int64_t& qty )
{
    std::vector<Trade> trades;

    while( qty > 0 && !bids.empty() )
    {
        auto it = bids.rbegin();
        auto cur_bid = it->first;

        if( prc && prc > cur_bid )
        {
            break;
        }

        auto order = it->second->head;
        auto filled_qty = std::min( qty, order->qty );
        qty -= filled_qty;

        trades.emplace_back(order->id, prc.value_or(0), filled_qty);

        if( filled_qty == order->qty )
        {
            //order is completed executed, we can remove this order
            remove_order( order );
        }
        else
        {

            order->qty -= filled_qty;
        }
    }

    return trades;
}

std::vector<Trade> OrderBook::match_sell_side( std::optional<int64_t> prc, int64_t& qty )
{
    std::vector<Trade> trades;

    while( qty > 0 && !asks.empty() )
    {
        auto it = asks.begin();

        auto cur_ask = it->first;

        if( prc && prc < cur_ask )
        {
            break;
        }

        auto order = it->second->head;
        auto filled_qty = std::min( qty, order->qty );
        qty -= filled_qty;

        trades.emplace_back(order->id, prc.value_or(0), filled_qty);

        if( filled_qty == order->qty )
        {
            remove_order( order );
        }
        else
        {
            order->qty -= filled_qty;
        }

    }

    return trades;
}



std::vector<Trade> OrderBook::add_order( uint64_t order_id, Side side, int64_t prc, int64_t qty )
{
    std::vector<Trade> trades;

    if( side == Side::Buy )
    {
        trades = match_sell_side( prc, qty );
    }
    else
    {
        trades = match_buy_side( prc, qty );
    }

    if( qty > 0 )
    {
        insert_order( order_id, side, prc, qty );
    }

    return trades;
}

std::vector<Trade> OrderBook::modify_order(uint64_t order_id, int64_t prc, int64_t qty)
{
    if(auto it = orders.find(order_id); it != std::end(orders))
    {
        if(qty  == 0)
        {
            remove_order(it->second.get());
            return {};
        }

        auto order = it->second.get();

        if(prc == order->prc && qty < order->qty)
        {
            order->ptr_level->total_qty -= order->qty - qty;
            order->qty = qty;
            return {};
        }

        auto side = order->side;
        remove_order(order);
        return add_order(order_id, side, prc, qty);
    }

    return {};
}

void OrderBook::cancel_order(uint64_t order_id)
{
    if(auto it = orders.find(order_id); it != std::end(orders))
    {
        auto order = it->second.get();
        remove_order(order);
    }
}

void OrderBook::print_levels( const level_map_t& levels )
{
    fmt::memory_buffer buffer;
    auto out = std::back_inserter( buffer );

    if( levels.empty() )
        fmt::format_to( out, "(no levels)\n" );

    for( const auto& [price, level] : levels )
    {
        if( !level )
        {
            fmt::format_to( out, "Price: {} | <null level>\n", price );
            continue;
        }

        fmt::format_to( out,
                        "Price: {} | Total qty: {} | Orders: {}\n"
                        "  {:>20} {:>16} {:>16}\n",
                        price, level->total_qty, level->num_orders,
                        "Order ID", "Price", "Qty" );

        for( const Order* order = level->head;
                order != nullptr;
                order = order->next )
        {
            fmt::format_to( out, "  {:>20} {:>16} {:>16}\n",
                            order->id, order->prc, order->qty );
        }

        fmt::format_to( out, "\n" );
    }

    fmt::print( "{}", fmt::string_view( buffer.data(), buffer.size() ) );
}

void OrderBook::print_bids()
{
    fmt::println("-----BID LEVELS-----");
    print_levels(bids);
}

void OrderBook::print_asks()
{
    fmt::println("-----ASK LEVELS-----");
    print_levels(asks);
}

std::vector<Depth> OrderBook::top(Side side, int n_levels)
{
    std::vector<Depth> levels;
    if(side == Side::Buy)
    {
        for(auto it = bids.rbegin();  it != bids.rend() && std::cmp_less(ssize(levels), n_levels); ++it)
        {
            auto&& [prc, level] = *it;
            levels.emplace_back(prc, level->total_qty, level->num_orders);
        }

        return levels;
    }
    else if(side == Side::Sell)
    {
        for(auto it = asks.begin();  it != asks.end() && std::cmp_less(ssize(levels), n_levels); ++it)
        {
            auto&& [prc, level] = *it;
            levels.emplace_back(prc, level->total_qty, level->num_orders);
        }

        return levels;
    }

    return {};
}

}