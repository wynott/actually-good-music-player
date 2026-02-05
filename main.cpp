#include "app.h"

int main()
{
    // todo: move logging init here

    auto& app = ActuallyGoodMP::instance();
    app.init();
    app.run();
    app.shutdown();
}
