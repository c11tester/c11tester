// Copyright (c) 2006-2018 Maxim Khizhinsky
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef CDSUNIT_QUEUE_TEST_INTRUSIVE_MSQUEUE_H
#define CDSUNIT_QUEUE_TEST_INTRUSIVE_MSQUEUE_H

namespace cds_test {

    class intrusive_msqueue
    {
    public:
        template <typename Base>
        struct base_hook_item : public Base
        {
            int nVal;
            int nDisposeCount;

            base_hook_item()
                : nDisposeCount( 0 )
            {}

            base_hook_item( base_hook_item const& s)
                : nVal( s.nVal )
                , nDisposeCount( s.nDisposeCount )
            {}
        };

        template <typename Member>
        struct member_hook_item
        {
            int nVal;
            int nDisposeCount;
            Member hMember;

            member_hook_item()
                : nDisposeCount( 0 )
            {}

            member_hook_item( member_hook_item const& s )
                : nVal( s.nVal )
                , nDisposeCount( s.nDisposeCount )
            {}
        };

        struct mock_disposer
        {
            template <typename T>
            void operator ()( T * p )
            {
                ++p->nDisposeCount;
            }
        };

    };

} // namespace cds_test

#endif // CDSUNIT_QUEUE_TEST_INTRUSIVE_MSQUEUE_H
