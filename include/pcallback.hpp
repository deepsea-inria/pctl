#include <assert.h>

/***********************************************************************/

#ifndef _PCTL_PCALLBACK_H_
#define _PCTL_PCALLBACK_H_

namespace pasl {
namespace pctl {
namespace callback {

class client {
public:
  virtual void init() = 0;
  
  virtual void destroy() = 0;
  
  virtual void output() = 0;
};

typedef client* client_p;

/*!\class myset
 * \brief A data structure to represent a set of a given type.
 *
 * \remark The maximum size of the set is determined by the template
 * parameter max_sz.
 *
 */
template <class Elt, int max_sz>
class myset {
private:
  int i;
  Elt inits[max_sz];
  
public:
  int size() {
    return i;
  }
  
  void push(Elt init) {
    if (i >= max_sz)
      assert("need to increase max_sz\n");
    inits[i] = init;
    i++;
  }
  
  Elt peek(size_t i) {
    assert(i < size());
    return inits[i];
  }
  
  Elt pop() {
    i--;
    Elt x = inits[i];
    inits[i] = Elt();
    return x;
  }

};

myset<client_p, 2048> callbacks;
  
void init() {
  for (int i = 0; i < callbacks.size(); i++) {
    client_p callback = callbacks.peek(i);
    callback->init();
  }
}

void output() {
  for (int i = 0; i < callbacks.size(); i++) {
    client_p callback = callbacks.peek(i);
    callback->output();
  }
}

void destroy() {
  while (callbacks.size() > 0) {
    client_p callback = callbacks.pop();
    callback->destroy();
  }
}

void register_client(client_p c) {
  callbacks.push(c);
}

} //end namespace
} //end namespace
} //end namespace

#endif /*! _PCTL_PCALLBACK_H_ !*/