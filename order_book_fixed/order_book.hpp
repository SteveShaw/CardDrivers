#pragma once

#include <cstdint>
#include <map>
#include <unordered_map>
#include <memory>
#include <optional>

#define FMT_HEADER_ONLY
#include <fmt/core.h>

#include <vector>

namespace cpplib
{


enum class Side : uint8_t
{
    Buy,
    Sell
};

struct Order;
struct Level;

using level_map_t = std::map<int64_t, std::unique_ptr<Level>>;
using order_dict_t = std::unordered_map<uint64_t, std::unique_ptr<Order>>;
using level_iter_t = level_map_t::iterator;

struct Order
{
    uint64_t id {};
    Side side {};
    int64_t prc {};
    int64_t qty {};
    Level* ptr_level {};

    Order* next {};
    Order* prev {};
};

struct Level
{
    Order* head {nullptr};
    Order* tail {nullptr};

    int64_t total_qty {};
    uint32_t num_orders {};
    level_iter_t level_pos {};
};

struct Trade
{
    uint64_t order_id {};
    int64_t prc {};
    int64_t qty {};
};

struct Depth
{
    int64_t prc;
    int64_t qty;
    uint32_t num_orders;
};


class OrderBook
{
    private:

    level_map_t bids;
    level_map_t asks;

    order_dict_t orders;
    void insert_order(uint64_t order_id, Side side, int64_t prc, int64_t qty);



    void remove_order(Order* order);
    std::vector<Trade> match_buy_side(std::optional<int64_t> prc, int64_t& qty);
    std::vector<Trade> match_sell_side(std::optional<int64_t> prc, int64_t& qty);

    Level* insert_or_get_level(level_map_t& level_map, int64_t prc);
    void check_capacity(Side side, int64_t prc, int64_t qty,
                        const Order* replacing = nullptr) const;


public:

    // Positive limit prices; negative quantities and duplicate live IDs throw
    // invalid_argument. Capacity overflow throws overflow_error before mutation.
    // Zero add is a no-op; zero modify cancels. Executions use resting prices.
    // Same-price reductions and unchanged modifications retain FIFO priority.

    std::vector<Trade> add_order(uint64_t order_id, Side side, int64_t prc, int64_t qty);
    std::vector<Trade> modify_order(uint64_t order_id, int64_t prc, int64_t qty);
    void cancel_order(uint64_t order_id);
    std::vector<Depth> top(Side side, int n_levels = 5);

    static void print_levels(const level_map_t&);

    void print_bids();
    void print_asks();



};
}
