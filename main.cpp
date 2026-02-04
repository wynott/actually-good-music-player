#include "app.h"

int main()
{
    // todo: move logging init here

    ActuallyGoodMP player;
    player.init();
    player.run();
    player.shutdown();
}
