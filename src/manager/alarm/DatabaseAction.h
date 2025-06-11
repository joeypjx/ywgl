// DatabaseAction.h
#pragma once
#include "IAlarmAction.h"
#include "AlarmEventRepository.h"
#include "AlarmRule.h"
#include <memory>

class DatabaseAction : public IAlarmAction {
private:
    std::shared_ptr<AlarmEventRepository> repository_;

public:
    DatabaseAction(std::shared_ptr<AlarmEventRepository> repo)
        : repository_(std::move(repo)) {}

    // 注意：我们将修改 AlarmManager 来传递更丰富的上下文，但为保持接口稳定，
    // 这里我们先只用已有信息。一个更好的方案是修改IAlarmAction接口。
    // 在此我们通过构造函数来区分事件类型。
    void execute(const AlarmRule& rule) override {
        // 在实际场景中，我们可能想记录更多细节，例如触发的实际值。
        // 这需要扩展IAlarmAction接口，或在AlarmManager中获取值并传递。
        std::string details = "Event recorded via DatabaseAction.";
        repository_->insertEvent(rule, rule.isTriggeredState ? AlarmEventType::TRIGGERED : AlarmEventType::RECOVERED);
    }

    std::string getDescription() const override {
        return "Database";
    }
};