// Copyright (c) 2011, Cornell University
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//     * Neither the name of HyperDex nor the names of its contributors may be
//       used to endorse or promote products derived from this software without
//       specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

// STL
#include <string>

// po6
#include <po6/net/ipaddr.h>
#include <po6/net/location.h>

// e
#include <e/convert.h>
#include <e/timer.h>

// HyperDex
#include <hyperdex/client.h>

static int
usage();

int
main(int argc, char* argv[])
{
    if (argc != 5)
    {
        return usage();
    }

    po6::net::ipaddr ip;
    uint16_t port;
    std::string space = argv[3];
    uint32_t numbers;

    try
    {
        ip = argv[1];
    }
    catch (std::invalid_argument& e)
    {
        std::cerr << "The IP address must be an IPv4 or IPv6 address." << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        port = e::convert::to_uint16_t(argv[2]);
    }
    catch (std::domain_error& e)
    {
        std::cerr << "The port number must be an integer." << std::endl;
        return EXIT_FAILURE;
    }
    catch (std::out_of_range& e)
    {
        std::cerr << "The port number must be suitably small." << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        numbers = e::convert::to_uint32_t(argv[4]);
    }
    catch (std::domain_error& e)
    {
        std::cerr << "The number must be an integer." << std::endl;
        return EXIT_FAILURE;
    }
    catch (std::out_of_range& e)
    {
        std::cerr << "The number must be suitably small." << std::endl;
        return EXIT_FAILURE;
    }

    try
    {
        hyperdex::client cl(po6::net::location(ip, port));
        e::buffer one("one", 3);
        e::buffer zero("zero", 3);

        for (uint32_t num = 0; num < numbers; ++num)
        {
            e::buffer key;
            key.pack() << num;
            std::vector<e::buffer> value;

            for (size_t i = 0; i < 32; ++i)
            {
                value.push_back(e::buffer());

                if ((num & (1 << i)))
                {
                    value.back() = one;
                }
                else
                {
                    value.back() = zero;
                }
            }

            switch (cl.put(space, key, value))
            {
                case hyperdex::SUCCESS:
                    break;
                case hyperdex::NOTFOUND:
                    std::cerr << "Put returned NOTFOUND." << std::endl;
                    break;
                case hyperdex::INVALID:
                    std::cerr << "Put returned INVALID." << std::endl;
                    break;
                case hyperdex::ERROR:
                    std::cerr << "Put returned ERROR." << std::endl;
                    break;
                default:
                    std::cerr << "Put returned unknown status." << std::endl;
                    break;
            }
        }

        e::sleep_ms(1, 0);
        std::cerr << "Starting searches." << std::endl;
        timespec start;
        timespec end;

        clock_gettime(CLOCK_REALTIME, &start);

        for (uint32_t num = 0; num < numbers; ++num)
        {
            hyperdex::search terms(32);
            e::buffer key;
            key.pack() << num;

            for (size_t i = 0; i < 32; ++i)
            {
                if ((num & (1 << i)))
                {
                    terms.set(i, one);
                }
                else
                {
                    terms.set(i, zero);
                }
            }

            hyperdex::client::search_results s = cl.search(argv[3], terms);

            if (!s.valid())
            {
                std::cerr << "Number " << num << " found nothing." << std::endl;
            }
            else
            {
                if (key != s.key())
                {
                    std::cerr << "Number " << num << " returned wrong key: " << key.hex() << " " << s.key().hex() << std::endl;
                }

                s.next();

                if (s.valid())
                {
                    std::cerr << "Number " << num << " found more than one result." << std::endl;
                }
            }
        }

        clock_gettime(CLOCK_REALTIME, &end);
        timespec diff;

        if ((end.tv_nsec < start.tv_nsec) < 0)
        {
            diff.tv_sec = end.tv_sec - start.tv_sec - 1;
            diff.tv_nsec = 1000000000 + end.tv_nsec - start.tv_nsec;
        }
        else
        {
            diff.tv_sec = end.tv_sec - start.tv_sec;
            diff.tv_nsec = end.tv_nsec - start.tv_nsec;
        }

        uint64_t nanosecs = diff.tv_sec * 1000000000 + diff.tv_nsec;
        std::cerr << "test took " << nanosecs << " nanoseconds for " << numbers << " searches" << std::endl;
    }
    catch (po6::error& e)
    {
        std::cerr << "There was a system error:  " << e.what();
        return EXIT_FAILURE;
    }
    catch (std::runtime_error& e)
    {
        std::cerr << "There was a runtime error:  " << e.what();
        return EXIT_FAILURE;
    }
    catch (std::bad_alloc& e)
    {
        std::cerr << "There was a memory allocation error:  " << e.what();
        return EXIT_FAILURE;
    }
    catch (std::exception& e)
    {
        std::cerr << "There was a generic error:  " << e.what();
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

int
usage()
{
    std::cerr << "Usage:  binary <coordinator ip> <coordinator port> <space name> <numbers>\n"
              << "This will create <numbers> points whose key is a number [0, <numbers>) and "
              << "then perform searches over the bits of the number.  The space should have 32 "
              << "secondary dimensions so that all bits of a number may be stored."
              << std::endl;
    return EXIT_FAILURE;
}