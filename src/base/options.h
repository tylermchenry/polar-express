#ifndef OPTIONS_H
#define OPTIONS_H

#include <memory>
#include <string>
#include <vector>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>
#include <crypto++/secblock.h>

#include "base/macros.h"

namespace polar_express {
namespace options {

bool Init(int argc, char** argv, program_options::variables_map* vm);

namespace internal {

class OptionDefinition {
 public:
  static void AddAllDefinedOptions(program_options::options_description* desc);

 protected:
  OptionDefinition(const string& name, const string& description);
  virtual ~OptionDefinition();

  virtual void AddSelf(program_options::options_description* desc) const = 0;

  const string name_;
  const string description_;

 private:
  static unique_ptr<vector<const OptionDefinition*> > defined_options_;
};

template <typename T>
class OptionDefinitionTmpl : public OptionDefinition {
 public:
  OptionDefinitionTmpl(const string& name, const string& description,
                       const T& default_value, unique_ptr<T>* value);

 private:
  virtual void AddSelf(program_options::options_description* desc) const;

  const unique_ptr<T>* value_;
};

template <typename T>
OptionDefinitionTmpl<T>::OptionDefinitionTmpl(const string& name,
                                              const string& description,
                                              const T& default_value,
                                              unique_ptr<T>* value)
    : OptionDefinition(name, description),
      value_(CHECK_NOTNULL(value)) {
  *CHECK_NOTNULL(*value_) = default_value;
}

template <typename T>
void OptionDefinitionTmpl<T>::AddSelf(
    program_options::options_description* desc) const {
  desc->add_options()(name_.c_str(), program_options::value<T>(value_->get()),
                      description_.c_str());
}

}  // namespace internal
}  // namespace options
}  // namespace polar_express

// These macros must be used outside of any namespace block!
#define DEFINE_OPTION(name, type, default_value, description)         \
  namespace polar_express {                                           \
  namespace options {                                                 \
  namespace internal {                                                \
  unique_ptr<type> opt_##type_##name(new type);                       \
  OptionDefinitionTmpl<type> polar_express_options_definition_##name( \
      #name, (description), (default_value), &(opt_##type_##name));   \
  } /* namespace internal */                                          \
  static const type& name = *(internal::opt_##type_##name);           \
  } /* namespace options */                                           \
  } /* namespace_polar_express */

#define DECLARE_OPTION(name, type)                          \
  namespace polar_express {                                 \
  namespace options {                                       \
  namespace internal {                                      \
  extern unique_ptr<type> opt_##type_##name;                \
  } /* namespace internal */                                \
  static const type& name = *(internal::opt_##type_##name); \
  } /* namespace options */                                 \
  } /* namespace_polar_express */

#endif  // OPTIONS_H
