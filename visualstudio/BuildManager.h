#pragma once

class ProtoBotCommander;

class BuildManager{
public:
    ProtoBotCommander* commanderReference;

    BuildManager(ProtoBotCommander* commanderReference);

    void onStart();
    void onFrame();
};