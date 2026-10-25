#include "order_book.hpp"
#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Contract: executions use resting prices and report resting IDs; duplicate live
// IDs and invalid inputs are rejected (exception or no-op) before any mutation.
// Quantity-zero add is a no-op, quantity-zero modify cancels. Same-price
// reductions/no-op modifications retain FIFO; increases/repricing lose FIFO.
// Positive limit prices are assumed. No market-order API is exposed.
using namespace cpplib;
void check(bool ok, const char* expression, int line) {
    if (!ok) throw std::runtime_error(std::string("line ") + std::to_string(line) + ": " + expression);
}
#define CHECK(x) check(bool(x), #x, __LINE__)
void depth(OrderBook& b, Side s, std::initializer_list<Depth> expected, int n = 100) {
    const auto got = b.top(s, n);
    CHECK(got.size() == expected.size());
    size_t i = 0;
    for (const auto& e : expected) {
        CHECK(got[i].prc == e.prc); CHECK(got[i].qty == e.qty);
        CHECK(got[i].num_orders == e.num_orders); ++i;
    }
}
void trades(const std::vector<Trade>& got, std::initializer_list<Trade> expected) {
    CHECK(got.size() == expected.size()); size_t i = 0;
    for (const auto& e : expected) {
        CHECK(got[i].order_id == e.order_id); CHECK(got[i].prc == e.prc);
        CHECK(got[i].qty == e.qty); ++i;
    }
}
bool equal(const std::vector<Depth>& a, const std::vector<Depth>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i=0; i<a.size(); ++i)
        if(a[i].prc!=b[i].prc || a[i].qty!=b[i].qty || a[i].num_orders!=b[i].num_orders) return false;
    return true;
}
template<class F> void reject_unchanged(OrderBook& b, F operation) {
    auto bids=b.top(Side::Buy,100), asks=b.top(Side::Sell,100);
    try { CHECK(operation().empty()); } catch(const std::invalid_argument&) {}
      catch(const std::domain_error&) {} catch(const std::overflow_error&) {}
    CHECK(equal(bids,b.top(Side::Buy,100))); CHECK(equal(asks,b.top(Side::Sell,100)));
}
struct Test { std::string name; std::function<void()> run; };
int main(int argc, char** argv) {
    std::vector<Test> tests;
    auto test=[&](std::string name, std::function<void()> f){ tests.push_back({name,f}); };
    test("empty_and_unknown_ids", [] {
        OrderBook b; depth(b,Side::Buy,{}); depth(b,Side::Sell,{});
        b.cancel_order(77); trades(b.modify_order(77,100,2),{});
        depth(b,Side::Buy,{}); depth(b,Side::Sell,{});
    });
    for (Side s : {Side::Buy,Side::Sell}) {
        const std::string prefix=s==Side::Buy ? "bid_" : "ask_";
        const Side incoming=s==Side::Buy ? Side::Sell : Side::Buy;
        test(prefix+"non_crossing", [=] {
            OrderBook b; trades(b.add_order(1,s,100,5),{});
            int p=s==Side::Buy ? 101 : 99;
            trades(b.add_order(2,incoming,p,3),{});
            depth(b,s,{{100,5,1}}); depth(b,incoming,{{p,3,1}});
        });
        test(prefix+"partial_fill_depth", [=] {
            OrderBook b; b.add_order(1,s,100,10);
            trades(b.add_order(2,incoming,100,4),{{1,100,4}});
            depth(b,s,{{100,6,1}}); depth(b,incoming,{});
        });
        test(prefix+"full_fill_last_order", [=] {
            OrderBook b; b.add_order(1,s,100,5);
            trades(b.add_order(2,incoming,100,5),{{1,100,5}});
            depth(b,s,{}); depth(b,incoming,{}); b.cancel_order(1);
            trades(b.add_order(1,s,100,2),{}); depth(b,s,{{100,2,1}});
        });
        test(prefix+"fifo_and_partial_second", [=] {
            OrderBook b; b.add_order(1,s,100,3); b.add_order(2,s,100,5); b.add_order(3,s,100,7);
            trades(b.add_order(9,incoming,100,6),{{1,100,3},{2,100,3}});
            depth(b,s,{{100,9,2}});
            trades(b.add_order(10,incoming,100,3),{{2,100,2},{3,100,1}});
            depth(b,s,{{100,6,1}});
        });
        test(prefix+"multiple_levels_limit", [=] {
            OrderBook b; int best=s==Side::Buy?102:98, worst=s==Side::Buy?98:102;
            b.add_order(3,s,worst,7); b.add_order(2,s,100,5); b.add_order(1,s,best,3);
            trades(b.add_order(9,incoming,100,10),{{1,best,3},{2,100,5}});
            depth(b,s,{{worst,7,1}}); depth(b,incoming,{{100,2,1}});
        });
        test(prefix+"price_improvement", [=] {
            OrderBook b; b.add_order(1,s,100,8);
            int limit=s==Side::Buy?90:110;
            trades(b.add_order(9,incoming,limit,2),{{1,100,2}});
        });
        test(prefix+"partial_then_cancel", [=] {
            OrderBook b; b.add_order(1,s,100,10); b.add_order(2,s,100,5);
            b.add_order(9,incoming,100,4); b.cancel_order(1);
            depth(b,s,{{100,5,1}});
        });
        test(prefix+"cancel_head_middle_tail", [=] {
            for(uint64_t cancel : {1u,2u,3u}) {
                OrderBook b; for(uint64_t id : {1u,2u,3u}) b.add_order(id,s,100,2);
                b.cancel_order(cancel); b.cancel_order(cancel); depth(b,s,{{100,4,2}});
                std::vector<Trade> got=b.add_order(9,incoming,100,4);
                CHECK(got.size()==2); size_t i=0;
                for(uint64_t id : {1u,2u,3u}) if(id!=cancel) {
                    CHECK(got[i].order_id==id); CHECK(got[i].qty==2); CHECK(got[i].prc==100); ++i;
                }
                depth(b,s,{});
            }
        });
        test(prefix+"cancel_only_level", [=] {
            OrderBook b; b.add_order(1,s,100,3); b.cancel_order(1); depth(b,s,{});
            b.add_order(2,s,100,4); depth(b,s,{{100,4,1}});
        });
        test(prefix+"modify_reduce_preserves_fifo", [=] {
            OrderBook b; b.add_order(1,s,100,5); b.add_order(2,s,100,4);
            trades(b.modify_order(1,100,2),{}); depth(b,s,{{100,6,2}});
            trades(b.add_order(9,incoming,100,3),{{1,100,2},{2,100,1}});
        });
        test(prefix+"modify_noop_preserves_fifo", [=] {
            OrderBook b; b.add_order(1,s,100,5); b.add_order(2,s,100,4);
            trades(b.modify_order(1,100,5),{});
            trades(b.add_order(9,incoming,100,1),{{1,100,1}});
        });
        test(prefix+"modify_increase_loses_fifo", [=] {
            OrderBook b; b.add_order(1,s,100,2); b.add_order(2,s,100,3);
            trades(b.modify_order(1,100,5),{}); depth(b,s,{{100,8,2}});
            trades(b.add_order(9,incoming,100,4),{{2,100,3},{1,100,1}});
        });
        test(prefix+"modify_price_moves_level", [=] {
            OrderBook b; b.add_order(1,s,100,2); b.add_order(2,s,100,3);
            int better=s==Side::Buy?101:99;
            trades(b.modify_order(1,better,4),{}); depth(b,s,{{better,4,1},{100,3,1}});
        });
        test(prefix+"modify_crosses", [=] {
            OrderBook b; int distant=s==Side::Buy?90:110;
            b.add_order(1,s,distant,2); b.add_order(2,incoming,100,5);
            trades(b.modify_order(1,100,3),{{2,100,3}});
            depth(b,s,{}); depth(b,incoming,{{100,2,1}});
        });
        test(prefix+"modify_zero_cancels", [=] {
            OrderBook b; b.add_order(1,s,100,2);
            trades(b.modify_order(1,100,0),{}); depth(b,s,{});
        });
        test(prefix+"depth_order_and_limits", [=] {
            OrderBook b; for(int i=0;i<7;++i) b.add_order(i+1,s,100+i,i+1);
            b.add_order(20,s,103,10);
            auto all=b.top(s,100); CHECK(all.size()==7); CHECK(b.top(s).size()==5);
            CHECK(b.top(s,1).size()==1); CHECK(b.top(s,0).empty()); CHECK(b.top(s,-1).empty());
            for(int i=0;i<7;++i) {
                int offset=s==Side::Buy?6-i:i;
                CHECK(all[i].prc==100+offset);
                CHECK(all[i].qty==offset+1+(offset==3?10:0));
                CHECK(all[i].num_orders==(offset==3?2u:1u));
            }
        });
        test(prefix+"duplicate_non_crossing", [=] {
            OrderBook b; b.add_order(1,s,100,5);
            reject_unchanged(b,[&]{ return b.add_order(1,s,99,7); });
            trades(b.add_order(9,incoming,100,1),{{1,100,1}});
        });
        test(prefix+"duplicate_crossing", [=] {
            OrderBook b; b.add_order(1,s,100,5);
            reject_unchanged(b,[&]{ return b.add_order(1,incoming,100,2); });
        });
        test(prefix+"duplicate_crossing_other_order", [=] {
            OrderBook b; int distant=s==Side::Buy?90:110;
            b.add_order(1,s,distant,5); b.add_order(2,incoming,100,4);
            reject_unchanged(b,[&]{ return b.add_order(1,s,100,7); });
        });
        test(prefix+"zero_negative_add", [=] {
            OrderBook b; b.add_order(1,s,100,5);
            reject_unchanged(b,[&]{return b.add_order(2,incoming,100,0);});
            reject_unchanged(b,[&]{return b.add_order(2,incoming,100,-1);});
        });
        test(prefix+"negative_modify_same_price", [=] {
            OrderBook b; b.add_order(1,s,100,5);
            reject_unchanged(b,[&]{return b.modify_order(1,100,-1);});
        });
        test(prefix+"negative_modify_new_price", [=] {
            OrderBook b; b.add_order(1,s,100,5);
            reject_unchanged(b,[&]{return b.modify_order(1,99,-1);});
        });
        test(prefix+"invalid_prices", [=] {
            for(int64_t price : {int64_t(0),int64_t(-1)}) {
                OrderBook b; b.add_order(1,s,100,5);
                reject_unchanged(b,[&]{return b.add_order(2,s,price,2);});
                reject_unchanged(b,[&]{return b.modify_order(1,price,2);});
            }
        });
        test(prefix+"quantity_overflow", [=] {
            OrderBook b; b.add_order(1,s,100,std::numeric_limits<int64_t>::max());
            reject_unchanged(b,[&]{return b.add_order(2,s,100,1);});
        });
        test(prefix+"modify_increase_overflow_atomic", [=] {
            OrderBook b; auto max=std::numeric_limits<int64_t>::max();
            b.add_order(1,s,100,max-2); b.add_order(2,s,100,2);
            reject_unchanged(b,[&]{return b.modify_order(2,100,3);});
            trades(b.add_order(9,incoming,100,1),{{1,100,1}});
            depth(b,s,{{100,max-1,2}});
        });
        test(prefix+"modify_reprice_overflow_atomic", [=] {
            OrderBook b; auto max=std::numeric_limits<int64_t>::max();
            int worse=s==Side::Buy?99:101;
            b.add_order(1,s,100,max); b.add_order(2,s,worse,1);
            reject_unchanged(b,[&]{return b.modify_order(2,100,1);});
            b.cancel_order(2); depth(b,s,{{100,max,1}});
        });
        test(prefix+"modify_capacity_excludes_old_quantity", [=] {
            OrderBook b; auto max=std::numeric_limits<int64_t>::max();
            b.add_order(1,s,100,max-3); b.add_order(2,s,100,2);
            trades(b.modify_order(2,100,3),{}); depth(b,s,{{100,max,2}});
            trades(b.add_order(9,incoming,100,1),{{1,100,1}});
            depth(b,s,{{100,max-1,2}});
        });
        test(prefix+"extreme_price_quantity_and_ids", [=] {
            OrderBook b; auto max=std::numeric_limits<int64_t>::max();
            auto id=std::numeric_limits<uint64_t>::max();
            b.add_order(id,s,max,max); depth(b,s,{{max,max,1}});
            trades(b.add_order(0,incoming,max,max),{{id,max,max}}); depth(b,s,{});
            b.add_order(0,s,1,1); b.cancel_order(0); depth(b,s,{});
            b.add_order(id,s,1,1); b.cancel_order(id); depth(b,s,{});
        });
    }
    test("invalid_side_empty", [] {
        OrderBook b; reject_unchanged(b,[&]{return b.add_order(1,static_cast<Side>(255),100,2);});
        CHECK(b.top(static_cast<Side>(255)).empty());
    });
    test("invalid_side_crossing", [] {
        OrderBook b; b.add_order(1,Side::Buy,100,5);
        reject_unchanged(b,[&]{return b.add_order(2,static_cast<Side>(255),100,2);});
    });
    int passed=0, failed=0, selected=0;
    if(argc>1 && std::string(argv[1])=="--list") {
        for(const auto& t:tests) std::cout<<t.name<<'\n';
        return 0;
    }
    for(const auto& t:tests) {
        if(argc>1 && t.name!=argv[1]) continue;
        ++selected;
        try { t.run(); ++passed; std::cout<<"PASS "<<t.name<<std::endl; }
        catch(const std::exception& e) { ++failed; std::cerr<<"FAIL "<<t.name<<": "<<e.what()<<std::endl; }
    }
    std::cout<<passed<<" passed, "<<failed<<" failed, "<<selected<<" selected\n";
    return failed || selected==0 ? 1 : 0;
}
