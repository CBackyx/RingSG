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

void get_compare_select_Circ(
    BetaCircuit& cd,
    u64 elementSize
) {
    BetaLibrary lib;

    BetaBundle a_0(elementSize);
    BetaBundle a_1(elementSize);
    BetaBundle b_0(elementSize);
    BetaBundle b_1(elementSize);
    BetaBundle c(1);
    BetaBundle a_2(elementSize);
    BetaBundle b_2(elementSize);
    BetaBundle temp_0(elementSize);
    BetaBundle temp_1(elementSize);

    cd.addInputBundle(a_0);
    cd.addInputBundle(a_1);
    cd.addInputBundle(b_0);
    cd.addInputBundle(b_1);
    cd.addTempWireBundle(c);
    cd.addOutputBundle(a_2);
    cd.addOutputBundle(b_2);
    cd.addTempWireBundle(temp_0);
    cd.addTempWireBundle(temp_1);

    lib.lessThan_build(
      cd,
      a_0, 
      a_1, 
      c, 
      BetaLibrary::IntType::Unsigned,
      BetaLibrary::Optimized::Size
    );

    lib.multiplex_build(
      cd,
      a_0,
      a_1,  
      c, 
      a_2,
      temp_0
    );    

    lib.multiplex_build(
      cd,
      b_0,
      b_1,  
      c, 
      b_2,
      temp_1
    );  
}

void get_min_Circ(
    BetaCircuit& cd,
    u64 elementSize
) {
    BetaLibrary lib;

    BetaBundle a_0(elementSize);
    BetaBundle a_1(elementSize);
    BetaBundle c(1);
    BetaBundle a_2(elementSize);
    BetaBundle temp_0(elementSize);

    cd.addInputBundle(a_0);
    cd.addInputBundle(a_1);
    cd.addTempWireBundle(c);
    cd.addOutputBundle(a_2);
    cd.addTempWireBundle(temp_0);

    lib.lessThan_build(
      cd,
      a_0, 
      a_1, 
      c, 
      BetaLibrary::IntType::Unsigned,
      BetaLibrary::Optimized::Size
    );

    lib.multiplex_build(
      cd,
      a_0,
      a_1,  
      c, 
      a_2,
      temp_0
    );
}