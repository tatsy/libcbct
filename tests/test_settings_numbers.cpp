#include "Settings/SettingsImporterBase.h"

#include <gtest/gtest.h>

#include <string>
#include <utility>

namespace libcbct::tests {
namespace {
class NumericSettings : public SettingsImporterBase {
public:
    void set(std::string value) { data["number"] = std::move(value); }
};
}  // namespace

TEST(SettingsNumbers, UsesDefaultOnlyForMissingKeys) {
    const NumericSettings settings;
    EXPECT_EQ(settings.getInt("missing", 7), 7);
    EXPECT_FLOAT_EQ(settings.getFloat("missing", 1.25f), 1.25f);
}

TEST(SettingsNumbers, ParsesValidValues) {
    NumericSettings settings;
    settings.set("-42");
    EXPECT_EQ(settings.getInt("number"), -42);
    settings.set("2.5e2");
    EXPECT_FLOAT_EQ(settings.getFloat("number"), 250.0f);
}

TEST(SettingsNumbers, RejectsMalformedIntegerInsteadOfReturningDefault) {
    NumericSettings settings;
    for (const auto text : {"", "abc", "12abc", "1.5", "9999999999999999999999999"}) {
        SCOPED_TRACE(text);
        settings.set(text);
        EXPECT_THROW(settings.getInt("number", 7), InvalidArgument);
    }
}

TEST(SettingsNumbers, RejectsMalformedFloatInsteadOfReturningDefault) {
    NumericSettings settings;
    for (const auto text : {"", "abc", "1.5abc", "1e", "1e1000", "1e-1000"}) {
        SCOPED_TRACE(text);
        settings.set(text);
        EXPECT_THROW(settings.getFloat("number", 1.25f), InvalidArgument);
    }
}
}  // namespace libcbct::tests
