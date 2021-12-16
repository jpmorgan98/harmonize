





template<typename T>
struct PairEquivalent;

template<>
struct PairEquivalent<unsigned short>
{
	typedef unsigned int Type;
};


template<>
struct PairEquivalent<unsigned int>
{
	typedef unsigned long long int Type;
};


template<typename T>
struct PairPack
{

	typedef PairPack<T> Self;

	typedef typename PairEquivalent<T>::Type PairType;

	static const PairType RIGHT_MASK = std::numeric_limits<T>::max();
	static const size_t   HALF_WIDTH = std::numeric_limits<T>::digits;
	static const PairType LEFT_MASK  = RIGHT_MASK << (PairType) HALF_WIDTH;

	PairType data;

	__host__  __device__ T    get_left() {
		return (data >> HALF_WIDTH) & RIGHT_MASK;
	}

	__host__  __device__ void set_left(T val) {
		data &= RIGHT_MASK;
		data |= ((PairType) val) << (PairType) HALF_WIDTH;
	}

	__host__  __device__ T    get_right(){
		return data & RIGHT_MASK;
	}

	__host__  __device__ void set_right(T val){
		data &= LEFT_MASK;
		data |= val;
	}

	PairPack<T> () = default;

	__host__  __device__ PairPack<T> (T left, T right){
		data   = left;
		data <<= HALF_WIDTH;
		data  |= right;
	}

};








template <typename ADR_INNER_TYPE>
struct Adr
{

	typedef ADR_INNER_TYPE AdrType;

	static const AdrType null = std::numeric_limits<AdrType>::max();

	AdrType adr;


	Adr<AdrType>() = default;

	__host__ __device__ Adr<AdrType>(AdrType adr_val) : adr(adr_val) {}

	__host__ __device__ bool operator==(const Adr<AdrType>& link_adr){
		return (adr == link_adr.adr);
	}


	 __host__ __device__ bool is_null(){
		return (adr == null);
	}


};



template <typename ADR_TYPE>
struct PoolQueue;


template <typename ADR_TYPE>
struct PoolQueue <Adr<ADR_TYPE>>
{

	typedef ADR_TYPE AdrType;
	typedef Adr<AdrType> LinkAdrType;
	typedef PoolQueue<LinkAdrType> Self;

	typedef typename PairEquivalent<AdrType>::Type QueueType;
	PairPack<AdrType> pair;

	static const QueueType null       = std::numeric_limits<QueueType>::max();

	__host__ __device__ LinkAdrType get_head(){
		return pair.get_left();
	}

	/*
	// This function extracts the tail of the given queue
	*/
	__host__ __device__ LinkAdrType get_tail(){
		return pair.get_right();
	}

	/*
	// Enable default constructor
	*/
	PoolQueue<Adr<ADR_TYPE>>() = default;

	/*
	// This function concatenates two link addresses into a queue
	*/
	__host__ __device__ PoolQueue<Adr<ADR_TYPE>>
	(LinkAdrType head, LinkAdrType tail)
	{
		pair = PairPack<AdrType>(head.adr,tail.adr);
	}

	/*
	// This function sets the head of the queue to the given link address
	*/
	__host__ __device__ void set_head(LinkAdrType head){
		pair.set_left(head.adr);
	}

	/*
	// This function sets the tail of the queue to the given link address
	*/
	__host__ __device__ void set_tail(LinkAdrType tail){
		pair.set_right(tail.adr);
	}

	/*
	// Returns true if and only if queue is empty
	*/
	 __host__ __device__ bool is_null(){
		return (pair.data == Self::null);
	}


	__host__ __device__ static const Self null_queue() {
		Self result;
		result.pair.data = Self::null;
		return result;
	}

};







	
template<typename T, typename AdrType>
__host__ void mempool_check(T* arena, size_t arena_size, PoolQueue<AdrType>* pool, size_t pool_size){

	
	


}











template<typename T, typename INDEX> struct MemPool;

template<typename T, typename INDEX>
__global__ void mempool_init(MemPool<T,INDEX> mempool);

template<typename T, typename INDEX>
struct MemPool {

	typedef INDEX Index;
	typedef Adr<Index> AdrType;
	typedef PoolQueue<AdrType> QueueType;

	const unsigned int RETRY_COUNT = 32;

	union Link {
		T       data;
		AdrType next;
	};


	Link*    arena;
	AdrType  arena_size;

	QueueType* pool;
	AdrType    pool_size;


	__host__ void host_init()
	{
		arena  = host::hardMalloc<Link>     ( arena_size.adr );
		pool   = host::hardMalloc<QueueType>( pool_size .adr );
		mempool_init<<<256,32>>>(*this);
		cudaDeviceSynchronize();
		//next_print();
	}

	__host__ void host_free()
	{
		host::auto_throw( cudaFree( arena ) );
		host::auto_throw( cudaFree( pool  ) );
	}


	__host__ void next_print(){
		Link* host_copy = (Link*)malloc(sizeof(Link)*arena_size.adr);
		cudaMemcpy(host_copy,arena,sizeof(Link)*arena_size.adr,cudaMemcpyDeviceToHost);
		for(int i=0; i<arena_size.adr; i++){
			printf("%d,",host_copy[i].next.adr);
		}
	}


	__host__ MemPool<T,Index>( Index as, Index ps )
		: arena_size(as)
		, pool_size (ps)
	{}

	__device__ T& operator[] (Index index){
		return arena[index].data;
	}


	__device__ Index pop_front(QueueType& queue){
		AdrType result;
		if( queue.is_null() ){
			result.adr = AdrType::null;
		} else {
			result = queue.get_head();
			AdrType next = arena[result.adr].next;
			queue.set_head(next);
			if( next.is_null() ){
				queue.set_tail(next);
			} else if ( queue.get_tail() == result ){
				printf("ERROR: Final link does not have a null next.\n");
				queue.pair.data = QueueType::null;
				return result.adr;
			}
		}
		return result.adr;
	}


	__device__ QueueType join(QueueType dst, QueueType src){
		if( dst.is_null() ){
			return src;
		} else if ( src.is_null() ) {
			return dst;
		} else {
			AdrType left_tail_adr  = dst.get_tail();
			AdrType right_head_adr = src.get_head();
			AdrType right_tail_adr = src.get_tail();

			/*
			// Find last link in the dst queue and set succcessor to head of src queue.
			*/
			arena[left_tail_adr.adr].next = right_head_adr;

			/* Set the right half of the left_queue handle to index the new tail. */
			dst.set_tail(right_tail_adr);
			
			return dst;
		}
		//arena[dst.get_tail().adr].next = src.get_head();
		//dst.set_tail(src.get_tail());
		//return dst;
	}


	__device__ QueueType pull_queue(Index& pull_idx){	
		Index start_idx = pull_idx;
		//printf("Pulling from %d",start_idx);
		bool done = false;
		QueueType queue = QueueType::null_queue();
		__threadfence();
		for(Index i=start_idx; i<pool_size.adr; i++){
			queue.pair.data = atomicExch(&(pool[i].pair.data),QueueType::null);
			if( ! queue.is_null() ){
				done = true;
				pull_idx = i;
				//printf("{pulled(%d,%d) from %d}",queue.get_head().adr,queue.get_tail().adr,pull_idx);
				return queue;
			}
		}
		if( !done ){
			for(Index i=0; i<start_idx; i++){
				queue.pair.data = atomicExch(&(pool[i].pair.data),QueueType::null);
				if( ! queue.is_null() ){
					done = true;
					pull_idx = i;
					//printf("{pulled(%d,%d) from %d}",queue.get_head().adr,queue.get_tail().adr,pull_idx);
					return queue;
				}
			}
		}
		//printf("{pulled(%d,%d)}",queue.get_head().adr,queue.get_tail().adr);
		return queue;
	}


	
	__device__ void push_queue(QueueType queue, Index push_idx){	
		if( queue.is_null() ){
			return;
		}
		//printf("Pushing (%d,%d) into %d",queue.get_head().adr,queue.get_tail().adr,push_idx);
		unsigned int try_count = 0;
		while(true){
			__threadfence();
			QueueType swap;
			swap.pair.data = atomicExch(&(pool[push_idx].pair.data),queue.pair.data);
			if( swap.is_null() ){
				//printf("{%d tries}",try_count);
				return;
			}
			try_count++;
			queue.pair.data = atomicExch(&(pool[push_idx].pair.data),QueueType::null);
			queue = join(swap,queue);
		}
	}




	__device__ Index pull_span( Index* dst, Index count, unsigned int& rand_state ){
		
		Index result = 0;
		for(unsigned int t=0; t<RETRY_COUNT; t++){
			Index queue_index = random_uint(rand_state) % pool_size.adr;
			QueueType queue = pull_queue(queue_index);
			if( ! queue.is_null() ){
				for( ; result<count; result++){
					Index old = atomicCAS(&dst[result],AdrType::null,queue.get_head());
					if( old != AdrType::null ){
						continue;
					}
					AdrType adr = pop_front(queue);
					if( adr == AdrType::null ){
						break;
					}
					dst[result] = adr.data;
				}
				push_queue(queue,queue_index);
			}
			if( result >= count ){
				return result;
			}
		}
		return result;
	}

	__device__ void push_span( Index* src, Index count, unsigned int& rand_state ){
		Index first = AdrType::null;
		Index last  = AdrType::null;
		for( Index i=0; i<count; i++){
			if( src[i] != AdrType::null ){
				if( first == AdrType::null ){
					first = src[i];
				} else if( last  != AdrType::null ){
					arena[last].next = src[i];
				}
				last = src[i];
			}
		}
		if( last != AdrType::null ){
			arena[last].next = AdrType::null;
		}
		QueueType queue = QueueType(first,last);
		Index queue_index = random_uint(rand_state) % pool_size.adr;
		push_queue(queue,queue_index);
	}


	__device__ Index alloc(unsigned int& rand_state){

		for( unsigned int t=0; t<RETRY_COUNT; t++){
			Index queue_index = random_uint(rand_state) % pool_size.adr;
			QueueType queue = pull_queue(queue_index);
			if( ! queue.is_null() ){
				AdrType adr = pop_front(queue);
				push_queue(queue,queue_index);
				return adr.adr;
			}
		}
		return AdrType::null;
	}

	__device__ void free(Index index, unsigned int& rand_state){
		if( index == AdrType::null ){
			return;
		}
		Index queue_index = random_uint(rand_state) % pool_size.adr;
		arena[index].next = AdrType::null;
		__threadfence();
		QueueType queue = QueueType(AdrType(index),AdrType(index));
		push_queue(queue,queue_index);
	}

};



template<typename T, typename INDEX>
__global__ void mempool_init(MemPool<T,INDEX> mempool){

	typedef MemPool<T,INDEX> PoolType;
	typedef INDEX Index;
	
	typedef typename PoolType::AdrType   AdrType;
	typedef typename PoolType::QueueType QueueType;

	Index thread_count = blockDim.x * gridDim.x;
	Index thread_index = blockDim.x * blockIdx.x + threadIdx.x;

	Index span = mempool.arena_size.adr / mempool.pool_size.adr;
	

	Index limit = mempool.arena_size.adr;
	for(Index i=thread_index; i<limit; i+=thread_count){
		if( ( (i%span) == (span-1) ) || ( i == (limit-1) ) ){
			mempool.arena[i].next = AdrType::null;
		} else {
			mempool.arena[i].next = i+1;
		}
		//printf("(%d:%d)",i,mempool.arena[i].next.adr);
	}

	for(Index i=thread_index; i<mempool.pool_size.adr; i+=thread_count){
		Index last;
		if( ((i+1)*span-1) >= mempool.arena_size.adr ) {
			last = mempool.arena_size.adr - 1;
		} else {
			last = (i+1)*span-1;
		}
		mempool.pool [i] = QueueType(AdrType(i*span),AdrType(last));
		//printf("{%d:(%d,%d)}",i,mempool.pool[i].get_head().adr,mempool.pool[i].get_tail().adr);
	}

}




template<typename T, typename INDEX, INDEX SIZE>
struct MemCache {

	typedef INDEX Index;
	typedef Adr<Index> AdrType;
	typedef MemPool<T,Index> PoolType;


	PoolType* parent;
	Index indexes[SIZE];


	#if    __CUDA_ARCH__ < 600
		#define atomicExch_block atomicExch
		#define atomicCAS_block  atomicCAS
	#endif


	__device__ void initialize (PoolType& p) {
		if( current_leader() ){
			parent = &p;
			for ( unsigned int i=0; i<SIZE; i++ ) {
				indexes[i] = AdrType::null;
			}
		}
	}

	__device__ void finalize (unsigned int &rand_state) {
		if( current_leader() ){
			for ( unsigned int i=0; i<SIZE; i++ ) {
				if( indexes[i] != AdrType::null ){
					parent->free(indexes[i],rand_state);
				}
			}
		}
	}

	
	__device__ Index alloc(unsigned int& rand_state){
		for ( unsigned int i=threadIdx.x; i<SIZE; i++ ) {
			Index swap = atomicExch_block(&(indexes[i]),AdrType::null);
			if( swap != AdrType::null ){
				return swap;
			}
		}
		for ( unsigned int i=0; i<threadIdx.x; i++ ) {
			Index swap = atomicExch_block(&(indexes[i]),AdrType::null);
			if( swap != AdrType::null ){
				return swap;
			}
		}
		return parent->alloc(rand_state);
	}

	__device__ void free(Index index, unsigned int& rand_state){
		for ( unsigned int i=threadIdx.x; i<SIZE; i++ ) {
			Index swap = atomicCAS_block(&(indexes[i]),AdrType::null,index);
			if( swap == AdrType::null ){
				return;
			}
		}
		for ( unsigned int i=0; i<threadIdx.x; i++ ) {
			Index swap = atomicCAS_block(&(indexes[i]),AdrType::null,index);
			if( swap == AdrType::null ){
				return;
			}
		}
		parent->free(index,rand_state);
	}


	#if    __CUDA_ARCH__ < 600
		#undef atomicExch_block
		#undef atomicCAS_block
	#endif


};





template<typename T, typename INDEX, typename MASK = unsigned int>
struct MemChunk {

	typedef INDEX Index;
	typedef MASK  Mask;
	typedef MemPool<MemChunk<T,Index,Mask>,Index> PoolType;

	static const Index SIZE = std::numeric_limits<Mask>::digits;
	static const Mask  FULL = std::numeric_limits<Mask>::max();
	static const MASK  HIGH_BIT = 1<<(SIZE-1);
	static const Index OFFSET_MASK = ~(SIZE-1);

	Mask fill_mask;
	T data[SIZE];
	
	__device__ Index offset(PoolType& parent){
		return (this - parent.arena);
	}

	__device__ bool full(){
		return (fill_mask == FULL);
	}
	
	__device__ bool empty(){
		return (fill_mask == 0);
	}
		
	__device__ bool fill_count(){
		return __popc(fill_mask);
	}
	
	__device__ MemChunk<T,Index,Mask>()
		: fill_mask(0)
	{}

	__device__ Index alloc(PoolType& parent, Mask& rand_state) {
		Mask try_mask = random_uint(rand_state);
		while( ! full() ){
			Mask scan_mask = try_mask & ~fill_mask;
			scan_mask = (scan_mask == 0) ? ~fill_mask : scan_mask;
			Index lead = leading_zeros(scan_mask);
			Mask select = HIGH_BIT >> lead;
			if( select == 0 ){
				break;
			}
			Mask prior = atomicOr(fill_mask,select);
			if( (prior & select) == 0 ){
				return ( SIZE * offset(parent) ) + ( SIZE - 1 - lead );
			}
		}
		return Adr<Index>::null;
	}

	__device__ void free(Index index, PoolType& parent, Mask& rand_state) {
		if( (index & OFFSET_MASK) == offset() ){
			Index inner_index = index & ~OFFSET_MASK;
			Mask  select = 1 << inner_index;
			atomicAnd(fill_mask, ~select);
		}
	}

};



template<typename T, typename INDEX, typename MASK, INDEX SIZE_VAL>
struct MemPoolBank {

	typedef INDEX Index;
	typedef MASK  MaskType;
	typedef MemChunk<T,Index,MaskType> ChunkType;
	typedef MemPool<ChunkType,Index>   PoolType;
	
	static const Index SIZE = SIZE_VAL;

	unsigned int rand_state;
	Index full_count;

	PoolType& parent;
	Index chunks[SIZE];

	__device__ Index containing_chunk(Index target){
		Index result = ChunkType::offset_mask & index;
		if( result >= parent.arena_size ){
			return Adr<Index>::null;
		}
		return result;
	}

	__device__ MemPoolBank<T,Index,MaskType,SIZE_VAL>
	(PoolType& parent_ref, unsigned int seed, bool& err)
		: parent(parent_ref)
		, full_count(0)
		, rand_state(seed)
	{
		for(int i=0; i<SIZE; i++){
			chunks[i] = Adr<Index>::null;
		}
		err = parent.pull_span(chunks,SIZE,seed);
	}


	__device__ Index alloc() {
		for(int i=0; i<SIZE; i++){
			if( chunks[i] == Adr<Index>::null ){
				continue;
			}
			
		}
	}

	__device__ Index hard_neighbor_alloc(Index target, MaskType& rand_state){
		return parent.arena[ChunkType::offset_mask & index].alloc(parent,rand_state);
	}

	__device__ Index neighbor_alloc(Index target, MaskType& rand_state){
		Index result = hard_neighbor_alloc(target,rand_state);
		if( result == Adr<Index>::null){
			return alloc(rand_state);
		} else {
			return result;
		}
	}


	__device__ Index coalesce(Index original) {
		
	}

	__device__ void free(Index index) {
		Index chunk_index = ChunkType::offset_mask & index;
		if( index < parent.arena_size ){
			parent.arena[chunk_index].free(parent,index);
		}
	}

};






// Experimental population control mechanism
#if 0
template<typename T, typename ID>
struct TitanicValue {

	PairPack<ID>

};




template<typename T, typename ITER_TYPE = unsigned int, typename HASH_TYPE = unsigned long long int>
struct TitanicIOBuffer {

	typedef ITER_TYPE IterType;
	typedef HASH_TYPE HashType;

	bool  mode;

	struct TitanicLink {
		IterType next;
		HashType hash;
		T        data;
	};

	IterType capacity;
	IterType overflow;

	AtomicIter<IterType> pull_exit_iter;


	__device__ ArrayIter<T,IterType> pull_span(IterType pull_size){
		Iter<IterType> pull_iter(0,0,0);
		if( mode ){
			pull_iter = iter.leap(pull_size);
		}
		return ArrayIter<T,IterType>(data,pull_iter);
	}

	__device__ ArrayIter<T,IterType> push(T value, unsigned int index, unsigned int priority){
		Iter<IterType> push_iter(0,0,0);
		if( !mode ){
			push_iter = iter.leap(push_size);
		}
		return ArrayIter<T,IterType>(data,push_iter);
	}

	__device__ void flip(){
		mode = !mode;	
		if(mode){
			iter = AtomicIter<IterType>(0,iter.value);
		} else {
			iter = AtomicIter<IterType>(0,0);
		}
	}

	__device__ bool empty() {
		return iter.done();
	}
	
};
#endif




