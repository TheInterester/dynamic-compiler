#pragma once

#include <vector>
#include <type_traits>
#include <functional>
#include <cstdint>

class CollectedHeap;

// Any object that inherits from collectable can be created and tracked by the
// garbage collector.
class Collectable
{
public:
  virtual ~Collectable() = default;
  virtual size_t getSize() { return 0; }

private:
  // Any private fields you add to the Collectable class will be accessible by
  // the CollectedHeap (since it is declared as friend below).  You can think of
  // these fields as the header for the object, which will include metadata that
  // is useful for the garbage collector.

  // NOTE: The Collectable person needs to add a 'marked_' field here
  // that the mark and sweep logic can use. Example:
  bool marked_ = false;

protected:
  /*
  The mark phase of the garbage collector needs to follow all pointers from the
  collectable objects, check if those objects have been marked, and if they
  have not, mark them and follow their pointers.  The simplest way to implement
  this is to require that collectable objects implement a follow method that
  calls heap.markSuccessors(...) on all collectable objects that this object
  points to.  markSuccessors() is the one responsible for checking if the
  object is marked and marking it.
  */
  virtual void follow(CollectedHeap &heap) = 0;

  friend CollectedHeap;
};

/*
  This class keeps track of the garbage collected heap. The class must:
  - provide and implement method(s) for allocating objects that will be
    supported by garbage collection
  - keep track of all currently allocated objects
  - provide and implement a method that performs mark and sweep to deallocate
    objects that are not reachable from a given set of objects
*/
class CollectedHeap
{
  std::vector<Collectable *> allocated_objects_;
  size_t memoryAllocated = 0;
  size_t memoryLimit;
  // Callback function to roots to begin garbage collection from so garbage collection can be trriggered from allocate() function
  std::function<std::vector<Collectable *>()> getRoots_;

public:
  /*
  This method allocates an object of type T, passing args to the constructor.
  T must be a subclass of Collectable.  Before returning the
  object, it should be registered so that it can be deallocated later.

  NOTE: The CollectedHeap person should implement this method.
  */
  CollectedHeap(size_t limit = SIZE_MAX) : memoryLimit(limit) {}

  ~CollectedHeap()
  {
    for (auto *obj : allocated_objects_)
    {
      delete obj;
    }
  }

  void setRoots(std::function<std::vector<Collectable *>()> callback)
  {
    getRoots_ = callback;
  }

  template <typename T, typename... Args>
  T *allocate(Args &&...args)
  {
    static_assert(std::is_constructible_v<T, Args...>);

    // Trigger GC if we're over 80% of memory limit
    // Avoid overflow: check memoryAllocated * 10 > memoryLimit * 8
    if (getRoots_ && memoryLimit != SIZE_MAX &&
        memoryAllocated * 10 > memoryLimit * 8)
    {
      auto roots = getRoots_();
      gc(roots.begin(), roots.end());
    }

    T *obj = new T(std::forward<Args>(args)...);

    allocated_objects_.push_back(obj);
    memoryAllocated += obj->getSize();

    return obj;
  }

  /*
  For performance reasons, you may also want to specialize the allocate
  methods to allocate particular kinds of objects...
  */

  /*
  ============================================================================
  MARK AND SWEEP LOGIC - START
  ============================================================================
  This is the method that is called by the follow(...) method of a Collectable
  object.  This is how a Collectable object lets the garbage collector know
  about other Collectable objects pointed to by itself.

  DEPENDENCIES:
  - Requires Collectable to have a 'marked_' field
  */
  void markSuccessors(Collectable *next)
  {
    // If the object is null or already marked, nothing to do
    if (next == nullptr || next->marked_)
    {
      return;
    }

    // Mark this object as reachable
    next->marked_ = true;

    // Recursively follow pointers from this object
    next->follow(*this);
  }

  /*
  The gc method should be periodically invoked by your VM (or by other methods
  in CollectedHeap) whenever the VM decides it is time to reclaim memory.  This
  method triggers the mark and sweep process.  How frequently the VM invokes
  this method should be computed based on the -mem parameter passed to the VM.
  Keep in mind, however, that your VM could be using some extra memory that is
  not managed by the garbage collector, so make sure you account for this.

  The Iterator type should support comparison, assignment and the ++ operator.
  It should also be able to dereference an iterator to get a Collectable
  object.  This method will take iterators marking the [begin, end) range of
  the rootset as arguments.

  DEPENDENCIES:
  - Requires Collectable to have a 'marked_' field
  - Requires CollectedHeap to have an 'allocated_objects_' container
  */
  template <typename Iterator>
void gc(Iterator begin, Iterator end)
{
    // MARK PHASE: Mark all reachable objects starting from the rootset
    for (Iterator it = begin; it != end; ++it)
    {
        Collectable *root = *it;
        markSuccessors(root);
    }

    // SWEEP PHASE: Compact and recompute memory
    size_t newMemory = 0;  // ✅ Start fresh
    auto out = allocated_objects_.begin();
    
    for (auto it = allocated_objects_.begin(); it != allocated_objects_.end(); ++it)
    {
        Collectable *obj = *it;
        
        if (!obj->marked_)
        {
            // Object is not reachable, delete it
            delete obj;
        }
        else
        {
            // Object is reachable, keep it
            obj->marked_ = false;
            *out++ = obj;
            newMemory += obj->getSize();  // ✅ Add to new total
        }
    }
    
    allocated_objects_.erase(out, allocated_objects_.end());
    memoryAllocated = newMemory;  // ✅ Set to recomputed value
}
/*
  ============================================================================
  MARK AND SWEEP LOGIC - END
  ============================================================================
  */

private:
  // NOTE: The CollectedHeap person needs to add a container here to track
  // all allocated objects. Example:
  // std::vector<Collectable *> allocated_objects_;
};
