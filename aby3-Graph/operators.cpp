#include "operators.h"

using namespace oc;
using namespace aby3;

void get_multiplex_Circ(
    BetaCircuit& cd,
    u64 elementSize
) {
    BetaLibrary lib;

    BetaBundle a(elementSize);
    BetaBundle b(elementSize);
    BetaBundle c(1);
    BetaBundle d(elementSize);
    BetaBundle temp(elementSize);

    cd.addInputBundle(a);
    cd.addInputBundle(b);
    cd.addInputBundle(c);
    cd.addOutputBundle(d);
    cd.addTempWireBundle(temp);

    lib.multiplex_build(
      cd,
      a,
      b,
      c,
      d,
      temp
    );
}