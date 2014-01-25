#ifndef OPTIONS_H
#define OPTIONS_H

#include <memory>
#include <string>
#include <vector>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include "base/macros.h"

namespace polar_express {
namespace options {

bool Init(int argc, char** argv, program_options::variables_map* vm);

namespace internal {

class OptionDefinition {
 public:
  static void AddAllDefinedOptions(program_options::options_description* desc);

 protected:
  OptionDefinition();
  virtual ~OptionDefinition();

  virtual void AddSelf(program_options::options_description* desc) const = 0;
  virtual const string& name() const = 0;

 private:
  static unique_ptr<vector<const OptionDefinition*> > defined_options_;
};

template <typename T>
class OptionDefinitionTmpl : public OptionDefinition {
 public:
  OptionDefinitionTmpl(const string& name, const string& description, T* value);

 protected:
  virtual void AddSelf(program_options::options_description* desc) const;
  virtual const string& name() const;

 private:
  const string name_;
  const string description_;
  T* value_;
};

template <typename T>
OptionDefinitionTmpl<T>::OptionDefinitionTmpl(const string& name,
                                              const string& description,
                                              T* value)
    : name_(name), description_(description), value_(CHECK_NOTNULL(value)) {
}

template <typename T>
void OptionDefinitionTmpl<T>::AddSelf(
    program_options::options_description* desc) const {
  desc->add_options()(name_.c_str(), program_options::value<T>(value_),
                      description_.c_str());
}

template <typename T>
const string& OptionDefinitionTmpl<T>::name() const {
  return name_;
}

}  // namespace internal
}  // namespace options
}  // namespace polar_express

// These macros must be used outside of any namespace block!
#define DEFINE_OPTION(name, type, default_value, description)         \
  namespace polar_express {                                           \
  namespace options {                                                 \
  namespace internal {                                                \
  type opt_##type_##name = (default_value);                           \
  OptionDefinitionTmpl<type> polar_express_options_definition_##name( \
      #name, (description), &(opt_##type_##name));                    \
  } /* namespace internal */                                          \
  static const type& name = options::internal::opt_##type_##name;     \
  } /* namespace options */                                           \
  } /* namespace_polar_express */

#define DECLARE_OPTION(name, type)                       \
  namespace polar_express {                              \
  namespace options {                                    \
  namespace internal {                                   \
  extern type opt_##type_##name;                         \
  } /* namespace internal */                             \
  static const type& name = internal::opt_##type_##name; \
  } /* namespace options */                              \
  } /* namespace_polar_express */

#endif  // OPTIONS_H
