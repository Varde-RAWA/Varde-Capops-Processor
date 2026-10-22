#include "ingest/VektorDecisionConsumer.hpp"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <unordered_map>

VektorDecisionConsumer::VektorDecisionConsumer(const Configuration &config)
    : redis_(config.getRedisUrl())
{
}

void VektorDecisionConsumer::ensureGroup()
{
    if (groupReady_)
        return;

    try
    {
        // Read existing entries too; create the stream if Ops has not yet.
        redis_.xgroup_create(stream_, group_, "0-0", true);
    }
    catch (const sw::redis::ReplyError &error)
    {
        // An existing group is expected after reconnecting/restarting.
        if (std::string(error.what()).find("BUSYGROUP") == std::string::npos)
            throw;
    }

    groupReady_ = true;
}

std::vector<VektorDecisionConsumer::Entry>
VektorDecisionConsumer::read(const std::string &id)
{
    std::unordered_map<std::string, std::vector<Entry>> streams;

    // No BLOCK argument: return immediately when there are no messages.
    redis_.xreadgroup(group_, consumer_, stream_, id, 32, std::inserter(streams, streams.end()));

    const auto found = streams.find(stream_);

    if (found == streams.end())
        return {};

    return std::move(found->second);
}

void VektorDecisionConsumer::poll(const Handler &handle)
{
    try
    {
        ensureGroup();

        auto entries = read("0-0");

        if (entries.empty())
            entries = read(">");

        std::vector<std::string> acknowledged;

        for (const auto &[id, fields] : entries)
        {
            if (!fields)
            {
                // A pending entry's payload may have been trimmed/deleted.
                std::cerr << "Ignoring deleted Vektor decision "
                          << id << '\n';
            }
            else
            {
                const auto data = std::find_if(
                    fields->begin(), fields->end(),
                    [](const auto &field)
                    {
                        return field.first == "data";
                    });

                varde::events::VektorDecisionEvent event;

                if (data == fields->end() ||
                    !event.ParseFromString(data->second) ||
                    event.metadata().version() != 3 ||
                    event.proposalid().empty())
                {
                    std::cerr << "Ignoring invalid Vektor decision "
                              << id << '\n';
                }
                else
                {
                    handle(event);
                }
            }

            acknowledged.push_back(id);
        }

        if (!acknowledged.empty())
        {
            redis_.xack(
                stream_, group_,
                acknowledged.begin(), acknowledged.end());
        }
    }
    catch (const sw::redis::Error &)
    {
        // Allow group recreation if Redis lost it during an outage/reset.
        groupReady_ = false;
        throw;
    }
}