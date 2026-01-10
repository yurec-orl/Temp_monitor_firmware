#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include "config.h"
#include "hardware.h"

// Invalid temperature value
constexpr float INVALID_TEMP = DEVICE_DISCONNECTED_C;

// Simple ring buffer for storing float values
// Fixed capacity of GRAPH_BUFFER_SIZE elements
class RingBuffer {
private:
  float buffer[GRAPH_BUFFER_SIZE];
  int head;      // Index where next element will be written
  int count;     // Current number of elements in buffer
  
public:
  // Constructor
  RingBuffer() : head(0), count(0) {
    // Initialize buffer with invalid temperature values
    for (int i = 0; i < GRAPH_BUFFER_SIZE; ++i) {
      buffer[i] = INVALID_TEMP;
    }
  }
  
  // Add new value to buffer
  // If buffer is full, oldest value is overwritten
  void add(float value) {
    buffer[head] = value;
    head = (head + 1) % GRAPH_BUFFER_SIZE;
    
    if (count < GRAPH_BUFFER_SIZE) {
      count++;
    }
  }
  
  // Get current number of elements in buffer
  int size() const {
    return count;
  }
  
  // Get element at index, counting from the latest (newest) element
  // Index 0 = newest element, Index size()-1 = oldest element
  // Returns INVALID_TEMP if index is out of range
  float get(int index) const {
    if (index < 0 || index >= count) {
      return INVALID_TEMP;  // Out of range
    }
    
    // Calculate actual position in buffer
    // Index 0 should return the newest element (last added)
    // The newest element is at (head - 1 + GRAPH_BUFFER_SIZE) % GRAPH_BUFFER_SIZE
    int actualIndex;
    if (count < GRAPH_BUFFER_SIZE) {
      // Not full: newest is at count-1, oldest at 0
      actualIndex = count - 1 - index;
    } else {
      // Full: newest is at head-1, go backwards
      actualIndex = (head - 1 - index + GRAPH_BUFFER_SIZE) % GRAPH_BUFFER_SIZE;
    }
    
    return buffer[actualIndex];
  }

  float getLatest() const {
    return get(0);
  }

  // Clear the buffer
  void clear() {
    head = 0;
    count = 0;
    for (int i = 0; i < GRAPH_BUFFER_SIZE; ++i) {
      buffer[i] = 0.0f;
    }
  }
  
  // Get capacity (maximum number of elements)
  int capacity() const {
    return GRAPH_BUFFER_SIZE;
  }
};

#endif // RING_BUFFER_H
