#include "scenes.h"
#include "steamgui.h"

SceneFuncStruct scenePaused(SteamGui* ui)
{
  ui->text("BLAAST");
  ui->text("(paused)");
  ui->text("");

  if(ui->button("Resume"))
    return { &sceneIngame };

  ui->button("Settings");

  if(ui->button("Quit"))
    return {};

  return { &scenePaused };
}

