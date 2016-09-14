#include <QDebug>
#include "cmnd/BasicCommands.h"
#include "cmnd/ScopedMacro.h"
#include "core/TimeKeyType.h"
#include "core/SRTKey.h"
#include "core/OpaKey.h"
#include "core/BoneKey.h"
#include "core/PoseKey.h"
#include "core/FFDKey.h"
#include "core/MeshKey.h"
#include "ctrl/TimeLineUtil.h"

using namespace core;

namespace ctrl
{

namespace TimeLineUtil
{

//---------------------------------------------------------------------------------------
MoveKey::MoveKey(TimeLineEvent& aCommandEvent)
    : mEvent(aCommandEvent)
    , mCurrent(0)
{
}

bool MoveKey::modifyMove(TimeLineEvent& aModEvent, int aAdd, const util::Range& aFrame)
{
    aModEvent.setType(TimeLineEvent::Type_MoveKey);

    // empty
    if (aAdd == 0)
    {
        return true;
    }

    // if next frames has a invalid value, nothing to do
    for (const TimeLineEvent::Target& target : mEvent.targets())
    {
        XC_ASSERT(!target.pos.isNull());

        int current = 0;
        int next = 0;

        if (mCurrent == 0)
        {
            current = target.pos.index();
            next = target.subIndex + aAdd;
        }
        else
        {
            current = target.subIndex;
            next = target.subIndex + aAdd;
        }

        if (next < aFrame.min() || aFrame.max() < next) return false;

        if (!target.pos.map().contains(current)) return false;

        if (target.pos.map().contains(next)) return false;
    }

    if (mCurrent == 0)
    {
        // this command is not executed
        for (TimeLineEvent::Target& target : mEvent.targets())
        {
            target.subIndex += aAdd;
        }
    }
    else
    {
        // this command was already executed
        for (TimeLineEvent::Target& target : mEvent.targets())
        {
            const int curr = target.subIndex;
            const int next = target.subIndex + aAdd;

            target.subIndex = next;
            aModEvent.pushTarget(*target.node, target.pos.type(), curr, next);

            // move target
            target.pos.line()->move(target.pos.type(), curr, next);
        }
    }
    return true;
}

void MoveKey::undo()
{
    for (TimeLineEvent::Target& target : mEvent.targets())
    {
        if (target.pos.index() == target.subIndex) continue;

        // move
        const int curr = target.subIndex;
        const int next = target.pos.index();
        const bool success = target.pos.line()->move(target.pos.type(), curr, next);
        XC_ASSERT(success); (void)success;
    }

    mCurrent = 0;
}

void MoveKey::redo()
{
    for (TimeLineEvent::Target& target : mEvent.targets())
    {
        if (target.pos.index() == target.subIndex) continue;

        // move
        const int curr = target.pos.index();
        const int next = target.subIndex;
        bool success = target.pos.line()->move(target.pos.type(), curr, next);
        XC_ASSERT(success); (void)success;
    }

    mCurrent = 1;
}

//---------------------------------------------------------------------------------------
template<class tKey, TimeKeyType tType>
void assignKeyData(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        const typename tKey::Data& aNewData, const QString& aText)
{
    XC_ASSERT(aTarget.timeLine());
    tKey* key = (tKey*)(aTarget.timeLine()->timeKey(tType, aFrame));
    XC_PTR_ASSERT(key);

    {
        cmnd::ScopedMacro macro(aProject.commandStack(), aText);

        auto notifier = new Notifier(aProject);
        notifier->event().setType(TimeLineEvent::Type_ChangeKeyValue);
        notifier->event().pushTarget(aTarget, tType, aFrame);
        macro.grabListener(notifier);

        auto command = new cmnd::Assign<typename tKey::Data>(&(key->data()), aNewData);
        aProject.commandStack().push(command);
    }
}

template<class tKey, TimeKeyType tType>
void assignKeyEasing(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        const util::Easing::Param& aNewData, const QString& aText)
{
    XC_ASSERT(aTarget.timeLine());
    tKey* key = (tKey*)(aTarget.timeLine()->timeKey(tType, aFrame));
    XC_PTR_ASSERT(key);

    {
        cmnd::ScopedMacro macro(aProject.commandStack(), aText);

        auto notifier = new Notifier(aProject);
        notifier->event().setType(TimeLineEvent::Type_ChangeKeyValue);
        notifier->event().pushTarget(aTarget, tType, aFrame);
        macro.grabListener(notifier);

        auto command = new cmnd::Assign<util::Easing::Param>(&(key->data().easing()), aNewData);
        aProject.commandStack().push(command);
    }
}

void assignSRTKeyData(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        const SRTKey::Data& aNewData)
{
    assignKeyData<SRTKey, TimeKeyType_SRT>(
                aProject, aTarget, aFrame, aNewData, "assign srt key");
}

void assignOpaKeyData(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        const OpaKey::Data& aNewData)
{
    assignKeyData<OpaKey, TimeKeyType_Opa>(
                aProject, aTarget, aFrame, aNewData, "assign opacity key");
}

void assignPoseKeyEasing(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        const util::Easing::Param& aNewData)
{
    assignKeyEasing<PoseKey, TimeKeyType_Pose>(
                aProject, aTarget, aFrame, aNewData, "assign pose key");
}

void assignFFDKeyEasing(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        const util::Easing::Param& aNewData)
{
    assignKeyEasing<FFDKey, TimeKeyType_FFD>(
                aProject, aTarget, aFrame, aNewData, "assign ffd key");
}

template<class tKey, TimeKeyType tType>
void pushNewKey(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        tKey* aKey, const QString& aText, TimeKey* aParentKey = nullptr)
{
    XC_PTR_ASSERT(aKey);
    XC_ASSERT(aTarget.timeLine());

    {
        cmnd::ScopedMacro macro(aProject.commandStack(), aText);

        // set notifier
        auto notifier = new Notifier(aProject);
        notifier->event().setType(TimeLineEvent::Type_PushKey);
        notifier->event().pushTarget(aTarget, tType, aFrame);
        macro.grabListener(notifier);

        // create commands
        auto& stack = aProject.commandStack();
        stack.push(new cmnd::GrabNewObject<tKey>(aKey));
        stack.push(aTarget.timeLine()->createPusher(tType, aFrame, aKey));

        if (aParentKey)
        {
            stack.push(new cmnd::PushBackTree<TimeKey>(&aParentKey->children(), aKey));
        }
    }
}

void pushNewSRTKey(
        Project& aProject, ObjectNode& aTarget, int aFrame, SRTKey* aKey)
{
    pushNewKey<SRTKey, TimeKeyType_SRT>(
                aProject, aTarget, aFrame, aKey, "push new srt key");
}

void pushNewOpaKey(
        Project& aProject, ObjectNode& aTarget, int aFrame, OpaKey* aKey)
{
    pushNewKey<OpaKey, TimeKeyType_Opa>(
                aProject, aTarget, aFrame, aKey, "push new opacity key");
}

void pushNewPoseKey(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        PoseKey* aKey, BoneKey* aParentKey)
{
    XC_PTR_ASSERT(aParentKey);
    pushNewKey<PoseKey, TimeKeyType_Pose>(
                aProject, aTarget, aFrame, aKey,
                "push new pose key", aParentKey);
}

void pushNewFFDKey(
        Project& aProject, ObjectNode& aTarget, int aFrame,
        FFDKey* aKey, MeshKey* aParentKey)
{
    pushNewKey<FFDKey, TimeKeyType_FFD>(
                aProject, aTarget, aFrame, aKey,
                "push new ffd key", aParentKey);
}

//-------------------------------------------------------------------------------------------------
Notifier* createMoveNotifier(Project& aProject,
                             ObjectNode& aTarget,
                             const TimeKeyPos& aPos)
{
    auto notifier = new Notifier(aProject);
    notifier->event().setType(TimeLineEvent::Type_MoveKey);
    notifier->event().pushTarget(aTarget, aPos);
    return notifier;
}

} // namespace TimeLineUtil

} // namespace ctrl