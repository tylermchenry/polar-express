#include "base/options.h"

#include <iostream>
#include <utility>

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/variables_map.hpp>

#include "base/make-unique.h"

namespace polar_express {
namespace options {
namespace {

const char kHelpOption[] = "help";
const char kVersionOption[] = "version";

const char kProgramName[] = "Polar Express";
const char kProgramDescription[] =
    "A tool for fast, efficient backups to Amazon Glacier.";
const char kVersionString[] = "0.1 alpha";
const char kBuildDate[] = __DATE__;
const char kCopyrightNotice[] = "Copyright (C) 2014 Tyler McHenry.";
const char kLicenseNotice[] =
    "License GPLv2+: GNU GPL version 2 or later "
    "<http://gnu.org/licenses/gpl.html>.\n"
    "This is free software: you are free to change and redistribute it.\n"
    "There is NO WARRANTY, to the extent permitted by law.";

}  // namespace

bool Init(int argc, char** argv) {
  program_options::variables_map vm;
  program_options::options_description desc("Options");
  desc.add_options()(kHelpOption, "Produce this message.");
  desc.add_options()(kVersionOption, "Show version information.");
  internal::OptionDefinition::AddAllDefinedOptions(&desc);

  // TODO: An option should be able to define itself as positional.
  program_options::positional_options_description pd;
  pd.add("backup_root", 1);

  program_options::store(
      program_options::parse_command_line(argc, argv, desc), vm);
  program_options::notify(vm);

  if (vm.count(kHelpOption)) {
    std::cout << "Usage: " << argv[0] << " [OPTION]..." << std::endl;
    std::cout << desc << std::endl;
    return false;
  } else if (vm.count(kVersionOption)) {
    std::cout << kProgramName << " (" << kVersionString << "): "
              << kProgramDescription << std::endl;
    std::cout << "Built on " << kBuildDate << "." << std::endl;
    std::cout << kCopyrightNotice << std::endl;
    std::cout << kLicenseNotice << std::endl;
    return false;
  }
  return true;
}

namespace internal {

// static
unique_ptr<map<string, const OptionDefinition*> >
    OptionDefinition::defined_options_;

OptionDefinition::OptionDefinition(const string& name,
                                   const string& description)
  : name_(name),
    description_(description) {
  assert(name != kHelpOption);
  assert(name != kVersionOption);
  if (!defined_options_) {
    defined_options_ = make_unique<map<string,const OptionDefinition*> >();
  }
  assert(defined_options_->find(name) == defined_options_->end());
  defined_options_->insert(make_pair(name, this));
}

OptionDefinition::~OptionDefinition() {
}

// static
void OptionDefinition::AddAllDefinedOptions(
    program_options::options_description* desc) {
  CHECK_NOTNULL(desc);
  if (!defined_options_) {
    return;
  }
  for (const auto& kv : *defined_options_) {
    const auto* opt = kv.second;
    opt->AddSelf(desc);
  }
}

}  // namespace internal
}  // namespace options
}  // namespace polar_express
