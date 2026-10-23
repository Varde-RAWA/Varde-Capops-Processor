#pragma once

#include "domain/AlertEvent.hpp"
#include "domain/VektorProposal.hpp"
#include "domain/types/ProcessingResult.hpp"

#include <string>

class IEventPublisher
{
public:
    virtual ~IEventPublisher() = default;

    virtual void publish(const ProcessingResult& result) = 0;
    virtual std::string publishAlert(const AlertEvent& event) = 0;
    virtual std::string publishProposal(const VektorProposal& proposal) = 0;
};
