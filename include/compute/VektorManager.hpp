#pragma once

#include "compute/VektorEngine.hpp"
#include "domain/AlertEvent.hpp"
#include "domain/VektorProposal.hpp"

#include <functional>
#include <map>
#include <optional>
#include <string>

enum class ProposalState
{
    Proposed,
    Approved,
    Rejected,
    Expired
};

class VektorManager
{
public:
    explicit VektorManager(std::function<std::string()> makeId);

    std::optional<VektorProposal> consider(
        const AlertEvent &alert,
        const Track &track,
        bool simulated,
        std::optional<Position> destination,
        const Zone &avoidance);

    // Returns a route only on the first valid approval.
    std::optional<VektorProposal> decide(
        const std::string &id,
        ProposalState decision);

    void closeAlert(const std::string &alertId);

    std::optional<ProposalState> state(const std::string &id) const;

private:
    struct Record
    {
        VektorProposal proposal;
        ProposalState state;
    };

    VektorEngine engine_;
    std::function<std::string()> makeId_;
    std::map<std::string, Record> proposals_;
    std::map<std::string, std::string> byAlert_;
};