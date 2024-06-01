#include "BoundedBuffer.h"



using namespace std;


BoundedBuffer::BoundedBuffer (int _cap) : cap(_cap) {
    // modify as needed
}

BoundedBuffer::~BoundedBuffer () {
    // modify as needed
}

void BoundedBuffer::push (char* msg, int size) {
    
    // 1. Convert the incoming byte sequence given by msg and size into a vector<char>
    //      use one of vector constructors
    vector<char> incoming(msg, msg + size/sizeof(char));
    // 2. Wait until there is room in the queue (i.e., queue lengh is less than cap)
    //      waiting on slot available
    unique_lock<mutex> ul(m);
    slot_available.wait(ul, [this] { return (q.size() < (size_t) cap); });
    
    q.push(incoming);
    
    ul.unlock();
    data_available.notify_one();
    
            
    
    // 3. Then push the vector at the end of the queue
    // 4. Wake up threads that were waiting for push
    //      notifying data available


}

int BoundedBuffer::pop (char* msg, int size) {
    // 1. Wait until the queue has at least 1 item
    //      waiting on data available
    // 2. Pop the front item of the queue. The popped item is a vector<char>
    // 3. Convert the popped vector<char> into a char*, copy that into msg; assert that the vector<char>'s length is <= size
    //      use vector::data()
    unique_lock<mutex> ul(m);
    data_available.wait(ul, [this] { return (q.size() >= 1); });
    
    
    vector<char> outgoing = q.front();
    
    char* tocopy = outgoing.data();
    
    
    for(long unsigned int i = 0; i < outgoing.size(); i++){
        *msg = *tocopy;
        msg++;
        tocopy++;
    }

    q.pop();
    
    ul.unlock();
    slot_available.notify_one();

    if(outgoing.size() <= (long unsigned int) size){
        return outgoing.size();
    }

    return size;
    
    
    
    // 4. Wake up threads that were waiting for pop
    //      notifying slot available
    // 5. Return the vector's length to the caller so that they know how many bytes were popped
}

size_t BoundedBuffer::size () {
    return q.size();
}
