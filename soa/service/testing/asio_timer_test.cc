// This file is part of MLDB. Copyright 2015 Datacratic. All rights reserved.

/* asio_timer_test.cc                                              -*- C++ -*-
   Jeremy Barnes, 20 June 2014
   Copyright (c) 2014 Datacratic Inc.  All rights reserved.


*/

#include "mldb/http/event_loop.h"
#include "mldb/http/asio_timer.h"
#include "mldb/http/asio_thread_pool.h"
#include "mldb/watch/watch_impl.h"
#include <boost/asio.hpp>
#include <thread>
#include "mldb/jml/utils/testing/watchdog.h"

#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK

#include <boost/test/unit_test.hpp>

using namespace std;
using namespace Datacratic;


// Disarm from a different thread whilst it's firing; check there is no
// deadlock
BOOST_AUTO_TEST_CASE( test_destroy_from_handler_no_deadlock )
{
    AsioThreadPool threads;
    threads.ensureThreads(4);
    getTimer(Date::now().plusSeconds(0.1), -1, threads.nextLoop()).wait();

    std::atomic<int> numFired(0);

    // Make sure the test bombs if there is a deadlock rather than hanging
    ML::Watchdog watchdog(5);

    auto timer = getTimer(Date::now().plusSeconds(0.01), -1, threads.nextLoop());
    timer.bind([&] (Date now) {  timer = WatchT<Date>();  ++numFired;  });

    for (int i = 0;  i < 10 && numFired == 0;  ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    BOOST_CHECK_EQUAL(numFired, 1);
}

BOOST_AUTO_TEST_CASE( test_rapid_creation_destruction_1 )
{
    AsioThreadPool threads;
    threads.ensureThreads(4);
    getTimer(Date::now().plusSeconds(0.1), -1, threads.nextLoop()).wait();

    Date future = Date::now().plusSeconds(1);

    std::atomic<int> numFired(0);

    for (unsigned i = 0;  i < 10000;  ++i) {
        auto timer = getTimer(future, -1, threads.nextLoop());
        timer.bind([&] (Date now) { ++numFired; });
        // now let it die
    }

    cerr << "numFired = " << numFired << endl;
}

BOOST_AUTO_TEST_CASE( test_rapid_creation_destruction_2 )
{
    AsioThreadPool threads;
    threads.ensureThreads(4);
    getTimer(Date::now().plusSeconds(0.1), -1, threads.nextLoop()).wait();

    Date past = Date::now();

    std::atomic<int> numFired(0);

    for (unsigned i = 0;  i < 10000;  ++i) {
        auto timer = getTimer(past, 1.0, threads.nextLoop());
        timer.bind([&] (Date now) { ++numFired; });
        // Again, let it die
    }

    cerr << "numFired = " << numFired << endl;
}

// Try to find a race condition between firing and destroying
BOOST_AUTO_TEST_CASE( test_rapid_creation_destruction_3 )
{
    AsioThreadPool threads;
    threads.ensureThreads(4);
    getTimer(Date::now().plusSeconds(0.1), -1, threads.nextLoop()).wait();

    std::atomic<int> numFired(0);

    for (unsigned i = 0;  i < 1000;  ++i) {
        auto timer = getTimer(Date::now().plusSeconds(0.00001), 0.1, threads.nextLoop());
        timer.bind([&] (Date now) { ++numFired; });
        getTimer(Date::now().plusSeconds(0.000001), -0.1, threads.nextLoop()).wait();
        //std::this_thread::yield();
        //timer.wait();
        // Again, let it die
    }

    cerr << "numFired = " << numFired << endl;
}

// Disarm from a different thread whilst it's firing
BOOST_AUTO_TEST_CASE( test_rapid_creation_destruction_4 )
{
    AsioThreadPool threads;
    threads.ensureThreads(4);
    getTimer(Date::now().plusSeconds(0.1), -1, threads.nextLoop()).wait();

    std::atomic<int> numFired(0);

    for (unsigned i = 0;  i < 100;  ++i) {
        auto timer = getTimer(Date::now().plusSeconds(0.00001), 0.00001,
                              threads.nextLoop());
        timer.bind([&] (Date now) { ++numFired; });
        
        std::atomic<bool> done(false);
        
        auto fn = [&] () { getTimer(Date::now().plusSeconds(0.0001), -1, threads.nextLoop()).wait();  timer = WatchT<Date>();  done = true; };
        std::thread t(fn);
        t.join();
    }

    cerr << "numFired = " << numFired << endl;
}
