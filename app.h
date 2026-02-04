#pragma once
#include "browser.h"

class ActuallyGoodMP
{
public:
    ActuallyGoodMP();
    void run();

private:
    // todo: add init_logging();

private:
    Browser _artist_browser;
    Browser _album_browser;
    Browser _song_browser;

};

