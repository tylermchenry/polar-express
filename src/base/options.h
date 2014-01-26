#ifndef OPTIONS_H
#define OPTIONS_H

#include <map>
#include <memory>
#include <string>

#include <boost/program_options/options_description.hpp>

#include "base/macros.h"

namespace polar_express {
namespace options {

bool Init(int argc, char** argv);

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
  static unique_ptr<map<string, const OptionDefinition*> > defined_options_;
};

template <typename T>
class OptionDefinitionTmpl : public OptionDefinition {
 public:
  OptionDefinitionTmpl(const string& name, const string& description,
                       const T& default_value,
                       const unique_ptr<T>* value);

 private:
  virtual void AddSelf(program_options::options_description* desc) const;

  const unique_ptr<T>* value_;
};

template <typename T>
OptionDefinitionTmpl<T>::OptionDefinitionTmpl(const string& name,
                                              const string& description,
                                              const T& default_value,
                                              const unique_ptr<T>* value)
    : OptionDefinition(name, description),
      value_(CHECK_NOTNULL(value)) {
  *CHECK_NOTNULL(*value_) = default_value;
}

template <typename T>
void OptionDefinitionTmpl<T>::AddSelf(
    program_options::options_description* desc) const {
  desc->add_options()(name_.c_str(),
                      program_options::value<T>(value_->get())
                          ->default_value(**value_)->value_name("<arg>"),
                      description_.c_str());
}

}  // namespace internal
}  // namespace options
}  // namespace polar_express

#define OPTION_VALUE_NAME(opt_name) opt_##opt_name
#define OPTION_DEFINITION_NAME(opt_name) def_opt_##opt_name

// These macros must be used outside of any namespace block!
#define DEFINE_OPTION(opt_name, opt_type, default_value, description)          \
  namespace polar_express {                                                    \
  namespace options {                                                          \
  namespace internal {                                                         \
  extern const unique_ptr<opt_type> OPTION_VALUE_NAME(opt_name)(new opt_type); \
  const OptionDefinitionTmpl<opt_type> OPTION_DEFINITION_NAME(opt_name)(       \
      #opt_name, (description), (default_value),                               \
      &(OPTION_VALUE_NAME(opt_name)));                                         \
  } /* namespace internal */                                                   \
  namespace {                                                                  \
  const opt_type& opt_name = *(internal::OPTION_VALUE_NAME(opt_name));         \
  } /* namespace */                                                            \
  } /* namespace options */                                                    \
  } /* namespace_polar_express */

#define DECLARE_OPTION(opt_name, opt_type)                             \
  namespace polar_express {                                            \
  namespace options {                                                  \
  namespace internal {                                                 \
  extern const unique_ptr<opt_type> OPTION_VALUE_NAME(opt_name);       \
  } /* namespace internal */                                           \
  namespace {                                                          \
  const opt_type& opt_name = *(internal::OPTION_VALUE_NAME(opt_name)); \
  } /* namespace */                                                    \
  } /* namespace options */                                            \
  } /* namespace_polar_express */

#endif  // OPTIONS_H
