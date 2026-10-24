#pragma once
#include <JuceHeader.h>
#include "OBSIntegrationService.h"

namespace LiveBus
{

class OBSGuideComponent : public juce::Component
{
public:
    explicit OBSGuideComponent(OBSIntegrationService& service);
    ~OBSGuideComponent() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onClose;

private:
    OBSIntegrationService& obsService;

    juce::Label titleLabel{"title", "OBS Studio Integration & Virtual Cable Routing Guide"};
    juce::TextButton closeBtn{"Close Guide"};
    juce::TextEditor guideText;

    void updateGuideContent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OBSGuideComponent)
};

} // namespace LiveBus
