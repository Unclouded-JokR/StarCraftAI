#pragma once
#include <BWAPI.h>
#include <variant>
#include <limits.h>
#include "../starterbot/Tools.h"

struct ResourceRequest;


/// <summary>
/// The SpenderManager is the way ProtoBot is able to reserve the resouces ProtoBot wants to spend and approve the requests of units ProtoBot is able to make. \n
/// This prevents ProtoBot from spending resoucres that ahve already been allocated to another request and allows ProtoBot to make decisions based on what it has already made.
/// </summary>
class SpenderManager
{
public:
    SpenderManager();

    /// <summary>
    /// Given the mineral and gas cost of a unit, considering the amount of minerals and gas we mightve spent this frame, are we able to afford this unit?
    /// </summary>
    /// <param name="mineralPrice"></param>
    /// <param name="gasPrice"></param>
    /// <param name="currentMinerals (minerals we have not spent considering the minerals we reserved)"></param>
    /// <param name="currentGas (gas we have not spent considering the gas we reserved)"></param>
    /// <returns></returns>
    bool canAfford(int mineralPrice, int gasPrice, int currentMinerals, int currentGas);

    /// <summary>
    /// Loops through all of ProtoBot's current ResourceRequest's and checks the amount of minerals we have already reserved for each unit compared the amount of minerals StarCraft says ProtoBot currently has.\n If a ResourceRequest has a State other than PendingApproval, we have already reserved the minerals for that request.
    /// </summary>
    /// <param name="ProtoBot ResourceRequests"></param>
    /// <returns>amount of minerals not reserved</returns>
    int availableMinerals(std::vector<ResourceRequest>&);

    /// <summary>
    /// Loops through all of ProtoBot's current ResourceRequest's and checks the amount of gas we have already reserved for each unit compared the amount of gas StarCraft says ProtoBot currently has.\n If a ResourceRequest has a State other than PendingApproval, we have already reserved the gas for that request.
    /// </summary>
    /// <param name="ProtoBot ResourceRequests"></param>
    /// <returns>amount of gas not reserved</returns>
    int availableGas(std::vector<ResourceRequest>&);

    /// <summary>
    /// Due to buildings having two states created and completed, this requires ProtoBot to check if we have enough supply being made already to prevent ProtoBot from being supply blocked.
    /// \n First ProtoBot adds up all the supply buildings and units that are approved in the ResourceRequest queue.
    /// \n Second a check to all the unit ProtoBot currently is accounted for. \n In StarCraft a building does not provide supply until it is in a completed state, to prevent ProtoBot from creating more supply when units are in a created but not completed state this second check prevents the case from happening.
    /// </summary>
    /// <param name="ProtoBot ResourceRequests"></param>
    /// <param name="ProtoBot units"></param>
    /// <returns>amount of supply not used</returns>
    int plannedSupply(std::vector<ResourceRequest>&, BWAPI::Unitset);
    int availableSupply();

    void printQueue();

    //BWAPI Events
    void onStart();

    /// <summary>
    /// The queue for ProtoBot's ResourceRequest's operates in a first in first out operation, this means that ProtoBot is not able to reorganize the queue in any advance ways but does provide a structed list of units to created in their requested order.\n\n
    /// The SpenderManager simply loops through the ResourceRequest's of ProtoBot and checks what the agent is able afford. Due to prioirty not being implemented a simple first pass is ran specifically for Pylons and Worker due to the units being integral to success.\n
    /// \n After the initial first pass the queue runs normally through the pending resource requests until we can no longer afford any more units.
    /// </summary>
    /// <param name="ProtoBot ResourceRequest"></param>
    void OnFrame(std::vector<ResourceRequest>&);
};
