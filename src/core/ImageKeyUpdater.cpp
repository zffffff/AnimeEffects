#include "core/ImageKeyUpdater.h"
#include "core/ImageKey.h"

namespace core
{

//-------------------------------------------------------------------------------------------------
class ImageResourceUpdaterBase : public cmnd::Stable
{
public:
    ImageResourceUpdaterBase(const ResourceUpdatingWorkspacePtr& aWorkspace,
                             bool aCreateTransitions)
        : mTargets()
        , mWorkspace(aWorkspace)
        , mCreateTransitions(aCreateTransitions)
    {
    }

    virtual ~ImageResourceUpdaterBase()
    {
    }

    virtual void exec()
    {
        for (auto& target : mTargets)
        {
            auto key = target.key;
            GridMesh::TransitionCreater transer(
                        key->data().gridMesh(),
                        key->data().resource()->pos());

            // update image
            target.key->setImage(target.nextImage);

            // create transition data
            if (mCreateTransitions)
            {
                auto& trans = mWorkspace->makeSureTransitions(key, key->data().gridMesh());
                trans = transer.create(
                            key->data().gridMesh().positions(),
                            key->data().gridMesh().vertexCount(),
                            key->data().resource()->pos());
            }
        }
        mWorkspace.reset(); // finish using
    }

    virtual void redo()
    {
        for (auto& target : mTargets)
        {
            target.key->setImage(target.nextImage);
        }
    }

    virtual void undo()
    {
        for (auto& target : mTargets)
        {
            target.key->setImage(target.prevImage);
        }
    }

protected:
    struct Target
    {
        Target(ImageKey* aKey)
            : key(aKey)
            , prevImage()
            , nextImage()
        {}
        ImageKey* key;
        img::ResourceHandle prevImage;
        img::ResourceHandle nextImage;
    };

    QList<Target>& targets() { return mTargets; }
    const QList<Target>& targets() const { return mTargets; }

private:
    QList<Target> mTargets;
    ResourceUpdatingWorkspacePtr mWorkspace;
    bool mCreateTransitions;

};

//-------------------------------------------------------------------------------------------------
class ImageReloader : public ImageResourceUpdaterBase
{
    TimeLine& mTimeLine;
    const ResourceEvent& mEvent;

    void tryPushTarget(ImageKey* aKey)
    {
        if (aKey)
        {
            auto node = mEvent.findTarget(aKey->data().resource()->serialAddress());
            if (node)
            {
                this->targets().push_back(Target(aKey));
                this->targets().back().prevImage = aKey->data().resource();
                this->targets().back().nextImage = node->handle();
            }
        }
    }

public:
    ImageReloader(TimeLine& aTimeLine, const ResourceEvent& aEvent,
                  const ResourceUpdatingWorkspacePtr& aWorkspace, bool aCreateTransitions)
        : ImageResourceUpdaterBase(aWorkspace, aCreateTransitions)
        , mTimeLine(aTimeLine)
        , mEvent(aEvent)
    {
    }

    virtual void exec()
    {
        // push default key
        tryPushTarget((ImageKey*)mTimeLine.defaultKey(TimeKeyType_Image));

        auto& map = mTimeLine.map(TimeKeyType_Image);
        for (auto itr = map.begin(); itr != map.end(); ++itr)
        {
            TimeKey* key = itr.value();
            TIMEKEY_PTR_TYPE_ASSERT(key, Image);

            // push key
            tryPushTarget((ImageKey*)key);
        }

        ImageResourceUpdaterBase::exec();
    }
};

//-------------------------------------------------------------------------------------------------
class ImageChanger : public ImageResourceUpdaterBase
{
public:
    ImageChanger(ImageKey& aKey, img::ResourceNode& aNewResource,
                 const ResourceUpdatingWorkspacePtr& aWorkspace, bool aCreateTransitions)
        : ImageResourceUpdaterBase(aWorkspace, aCreateTransitions)
    {
        this->targets().push_back(Target(&aKey));
        this->targets().back().prevImage = aKey.data().resource();
        this->targets().back().nextImage = aNewResource.handle();
    }
};

//-------------------------------------------------------------------------------------------------
cmnd::Stable* ImageKeyUpdater::createResourceUpdater(
        ObjectNode& aNode, const ResourceEvent& aEvent,
        const ResourceUpdatingWorkspacePtr& aWorkspace, bool aCreateTransitions)
{
    if (!aNode.timeLine()) return nullptr;
    return new ImageReloader(*aNode.timeLine(), aEvent, aWorkspace, aCreateTransitions);
}

cmnd::Stable* ImageKeyUpdater::createResourceUpdater(
        ImageKey& aKey, img::ResourceNode& aNewResource,
        const ResourceUpdatingWorkspacePtr& aWorkspace, bool aCreateTransitions)
{
    return new ImageChanger(aKey, aNewResource, aWorkspace, aCreateTransitions);
}

} // namespace core