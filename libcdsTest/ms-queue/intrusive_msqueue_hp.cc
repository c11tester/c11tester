#include "test_intrusive_msqueue.h"

#include <stdio.h>
#include <cds/init.h>
#include <cds/gc/hp.h>
#include <cds/intrusive/msqueue.h>
#include <vector>

#define NDEBUG		// disable assert()
#include <assert.h>
#include <atomic>

namespace ci = cds::intrusive;
typedef cds::gc::HP gc_type;

typedef cds_test::intrusive_msqueue base_class;
typedef typename base_class::base_hook_item< ci::msqueue::node<gc_type>> base_item_type;
typedef typename base_class::member_hook_item< ci::msqueue::node<gc_type>> member_item_type;

typedef cds_test::intrusive_msqueue::mock_disposer mock_disposer;

template <typename Queue, typename Data>
void test_enqueue( Queue& q, Data& arr )
{
    typedef typename Queue::value_type value_type;
    size_t nSize = arr.size();

    value_type * pv;
    for ( size_t i = 0; i < nSize; ++i )
	arr[i].nVal = static_cast<int>(i);

    assert(q.empty());
    assert(q.size() == 0);

    // pop from empty queue
//    pv = q.pop();
    assert( pv == nullptr );
    assert( q.empty());
    assert(q.size() == 0);

//    pv = q.dequeue();
    assert( pv == nullptr );
    assert( q.empty());
    assert(q.size() == 0);

    for ( size_t i = 0; i < nSize; ++i ) {
	if ( i & 1 )
	    q.push( arr[i] );
	else
	    q.enqueue( arr[i] );
	assert( !q.empty());
	assert(q.size() == i+1);
    }
}


template <typename Queue, typename Data>
void test_dequeue( Queue& q, Data& arr )
{
    typedef typename Queue::value_type value_type;
    size_t nSize = arr.size();

    value_type * pv;
/*
    for ( size_t i = 0; i < nSize; ++i )
	arr[i].nVal = static_cast<int>(i);

    assert(q.empty());
    assert(q.size() == 0);

    // pop from empty queue
    pv = q.pop();
    assert( pv == nullptr );
    assert( q.empty());
    assert(q.size() == 0);

    pv = q.dequeue();
    assert( pv == nullptr );
    assert( q.empty());
    assert(q.size() == 0);

    // push/pop test
    for ( size_t i = 0; i < nSize; ++i ) {
	if ( i & 1 )
	    q.push( arr[i] );
	else
	    q.enqueue( arr[i] );
	assert( !q.empty());
	assert(q.size() == i+1);
    }
*/

    for ( size_t i = 0; i < nSize; ++i ) {
	assert( !q.empty());
	assert( q.size() == nSize - i );
	if ( i & 1 )
	    pv = q.pop();
	else
	    pv = q.dequeue();
	assert( pv != nullptr );
	assert( pv->nVal == i);
    }
    assert( q.empty());
    assert( q.size() == 0 );

/*
    Queue::gc::scan();
    --nSize; // last element of array is in queue yet as a dummy item
    for ( size_t i = 0; i < nSize; ++i ) {
	assert( arr[i].nDisposeCount == 1 );
    }
    assert( arr[nSize].nDisposeCount == 0 );

    // clear test
    for ( size_t i = 0; i < nSize; ++i )
	q.push( arr[i] );

    assert( !q.empty());
    assert( q.size() == nSize );

    q.clear();
    assert( q.empty());
    assert( q.size() == 0 );

    Queue::gc::scan();
    for ( size_t i = 0; i < nSize - 1; ++i ) {
	printf("nDisCount (2): %d, (i) %lu\n",  arr[i].nDisposeCount, i );
    }
    printf("nDisCount: (1) %d\n",  arr[nSize - 1].nDisposeCount ); // this element is in the queue yet
    assert( arr[nSize].nDisposeCount == 1 );
*/

}

int main () {
	cds::Initialize();

	{
		typedef ci::MSQueue< gc_type, base_item_type > queue_type;	
		cds::gc::hp::GarbageCollector::Construct( queue_type::c_nHazardPtrCount, 1, 16 );
		cds::threading::Manager::attachThread();

		{
			typedef cds::intrusive::MSQueue< gc_type, base_item_type,
			    typename ci::msqueue::make_traits<
			        ci::opt::disposer< mock_disposer >
			        , cds::opt::item_counter< cds::atomicity::item_counter >
			        , ci::opt::hook< ci::msqueue::base_hook< ci::opt::gc<gc_type>>>
			    >::type
			> test_queue;

			std::vector<base_item_type> arr;
			arr.resize(5);
			printf("test start\n");
			{
				std::atomic<int> x;
				atomic_store_explicit(&x, 0xaaa, std::memory_order_seq_cst);
				test_queue q;
				test_enqueue(q, arr);
				atomic_store_explicit(&x, 0xccc, std::memory_order_seq_cst);
				test_dequeue(q, arr);
				atomic_store_explicit(&x, 0xbbb, std::memory_order_seq_cst);
			}
			printf("test end\n");

//			gc_type::scan();
//			check_array( arr );

		}

	}

	cds::Terminate();
}
