#include <gtest/gtest.h>
#include <cppcoro/task.hpp>
#include <type_traits>

#include "uvco.h"

namespace {
struct Dog {};
} // namespace

TEST(uvcoTypeTest, check) {
    ////// task.result

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::task<> &>().result()),
                            void>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::task<> &&>().result()),
                            void>));

    ASSERT_TRUE((std::is_same_v<
                 decltype(std::declval<uvco::task<void> &>().result()),
                 void>));

    ASSERT_TRUE((std::is_same_v<
                 decltype(std::declval<uvco::task<void> &&>().result()),
                 void>));

    ASSERT_TRUE((
            std::is_same_v<decltype(std::declval<uvco::task<int> &>().result()),
                           int &>));

    ASSERT_TRUE((std::is_same_v<
                 decltype(std::declval<uvco::task<int> &&>().result()),
                 int>));

    ASSERT_TRUE((
            std::is_same_v<decltype(std::declval<uvco::task<Dog> &>().result()),
                           Dog &>));

    ASSERT_TRUE((std::is_same_v<
                 decltype(std::declval<uvco::task<Dog> &&>().result()),
                 Dog &&>));

    ////// schedule

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<uvco::task<> &>())),
                            void>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<uvco::task<> &&>())),
                            void>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<uvco::task<void> &>())),
                            void>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<uvco::task<void> &&>())),
                            void>));
    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<uvco::task<int> &>())),
                            int &>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<uvco::task<int> &&>())),
                            int>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<uvco::task<Dog> &>())),
                            Dog &>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<uvco::task<Dog> &&>())),
                            Dog &&>));

    ////// schedule cppcoro::task

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<cppcoro::task<> &>())),
                            void>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<cppcoro::task<> &&>())),
                            void>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<cppcoro::task<void> &>())),
                            void>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<cppcoro::task<void> &&>())),
                            void>));
    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<cppcoro::task<int> &>())),
                            int &>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<cppcoro::task<int> &&>())),
                            int>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<cppcoro::task<Dog> &>())),
                            Dog &>));

    ASSERT_TRUE(
            (std::is_same_v<decltype(std::declval<uvco::scheduler>().schedule(
                                    std::declval<cppcoro::task<Dog> &&>())),
                            Dog &&>));
}
