extern "C" {
void foo();
void bar() {}
}

namespace bar_namespace {

void SomeExternalFunction();

void SomeFunction() {
  foo();
  SomeExternalFunction();
}

}                            
