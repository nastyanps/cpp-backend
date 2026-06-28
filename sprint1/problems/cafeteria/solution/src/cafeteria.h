#pragma once
#ifdef _WIN32
#include <sdkddkver.h>
#endif

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#include "hotdog.h"
#include "result.h"

namespace net = boost::asio;

using HotDogHandler = std::function<void(Result<HotDog> hot_dog)>;

// Класс для управления одним заказом хот-дога
class Order : public std::enable_shared_from_this<Order> {
public:
    Order(net::io_context& io, int id,
          std::shared_ptr<Sausage> sausage,
          std::shared_ptr<Bread> bread,
          std::shared_ptr<GasCooker> gas_cooker,
          HotDogHandler handler)
        : io_{io}
        , id_{id}
        , sausage_{std::move(sausage)}
        , bread_{std::move(bread)}
        , gas_cooker_{std::move(gas_cooker)}
        , handler_{std::move(handler)} {
    }

    void Execute() {
        // Запускаем приготовление сосиски и булки параллельно
        FrySausage();
        BakeBread();
    }

private:
    void FrySausage() {
        // Начинаем жарить сосиску — занимаем горелку
        sausage_->StartFry(*gas_cooker_, [self = shared_from_this()] {
            // Горелка занята, ставим таймер на 1.5 секунды
            self->sausage_timer_.expires_after(std::chrono::milliseconds{1500});
            self->sausage_timer_.async_wait([self](boost::system::error_code ec) {
                if (!ec) {
                    self->sausage_->StopFry();
                }
                self->CheckReadiness(ec);
            });
        });
    }

    void BakeBread() {
        // Начинаем печь булку — занимаем горелку
        bread_->StartBake(*gas_cooker_, [self = shared_from_this()] {
            // Горелка занята, ставим таймер на 1 секунду
            self->bread_timer_.expires_after(std::chrono::milliseconds{1000});
            self->bread_timer_.async_wait([self](boost::system::error_code ec) {
                if (!ec) {
                    self->bread_->StopBaking();
                }
                self->CheckReadiness(ec);
            });
        });
    }

    void CheckReadiness(boost::system::error_code ec) {
        if (delivered_) {
            return;
        }
        if (ec) {
            delivered_ = true;
            handler_(Result<HotDog>{std::make_exception_ptr(
                std::runtime_error{ec.message()})});
            return;
        }
        // Ждём пока оба ингредиента готовы
        if (!sausage_->IsCooked() || !bread_->IsCooked()) {
            return;
        }
        // Оба готовы — собираем хот-дог
        delivered_ = true;
        try {
            handler_(Result<HotDog>{HotDog{id_, sausage_, bread_}});
        } catch (...) {
            handler_(Result<HotDog>{std::current_exception()});
        }
    }

    net::io_context& io_;
    int id_;
    std::shared_ptr<Sausage> sausage_;
    std::shared_ptr<Bread> bread_;
    std::shared_ptr<GasCooker> gas_cooker_;
    HotDogHandler handler_;
    net::steady_timer sausage_timer_{io_};
    net::steady_timer bread_timer_{io_};
    bool delivered_ = false;
};

// Класс "Кафетерий"
class Cafeteria {
public:
    explicit Cafeteria(net::io_context& io)
        : io_{io} {
    }

    void OrderHotDog(HotDogHandler handler) {
        const int order_id = ++next_order_id_;
        auto sausage = store_.GetSausage();
        auto bread = store_.GetBread();

        std::make_shared<Order>(io_, order_id,
                                std::move(sausage),
                                std::move(bread),
                                gas_cooker_,
                                std::move(handler))->Execute();
    }

private:
    net::io_context& io_;
    Store store_;
    std::shared_ptr<GasCooker> gas_cooker_ = std::make_shared<GasCooker>(io_);
    int next_order_id_ = 0;
};