#include "base/options.h"

#include <iostream>

#include <boost/program_options/cmdline.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>

#include "base/make-unique.h"

namespace polar_express {
namespace options {

bool Init(int argc, char** argv, program_options::variables_map* vm) {
  CHECK_NOTNULL(vm);

  program_options::options_description desc("Options");
  desc.add_options()("help", "Produce this message.");
  internal::OptionDefinition::AddAllDefinedOptions(&desc);

  // TODO: An option should be able to define itself as positional.
  program_options::positional_options_description pd;
  pd.add("backup_root", 1);

  program_options::store(
      program_options::parse_command_line(argc, argv, desc), *vm);
  program_options::notify(*vm);

  if (vm->count("help")) {
    std::cout << desc << std::endl;
    return false;
  }
  return true;
}

namespace internal {

// static
unique_ptr<vector<const OptionDefinition*> > OptionDefinition::defined_options_;

OptionDefinition::OptionDefinition(const string& name,
                                   const string& description)
  : name_(name),
    description_(description) {
  if (!defined_options_) {
    defined_options_ = make_unique<vector<const OptionDefinition*> >();
  }
  defined_options_->push_back(this);
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
  for (const auto* opt : *defined_options_) {
    opt->AddSelf(desc);
  }
}

}  // namespace internal
}  // namespace options
}  // namespace polar_express
