#pragma once
// SAX preflight runs before allocating the DOM. Charges conservatively for
// container nodes/map entries and copied strings, in addition to input bytes.
namespace {
struct JsonBudget : nlohmann::json_sax<nlohmann::json> {
  size_t Remaining=128*1024*1024, Values=0;int Depth=0;std::string Error;
  bool Charge(size_t n){if(n>Remaining||++Values>2000000){Error="JSON memory/element budget exceeded";return false;}Remaining-=n;return true;}
  bool null() override{return Charge(32);}bool boolean(bool) override{return Charge(32);}
  bool number_integer(number_integer_t) override{return Charge(32);}bool number_unsigned(number_unsigned_t) override{return Charge(32);}
  bool number_float(number_float_t,const string_t&) override{return Charge(32);}
  bool string(string_t& s) override{return Charge(64+s.size()*2);}
  bool binary(binary_t& b) override{return Charge(64+b.size()*2);}
  bool start_object(std::size_t) override{if(++Depth>96){Error="JSON nesting budget exceeded";return false;}return Charge(128);}
  bool key(string_t& s) override{return Charge(128+s.size()*2);}
  bool end_object() override{--Depth;return true;}
  bool start_array(std::size_t) override{if(++Depth>96){Error="JSON nesting budget exceeded";return false;}return Charge(64);}
  bool end_array() override{--Depth;return true;}
  bool parse_error(std::size_t,const std::string&,const nlohmann::detail::exception& e) override{Error=e.what();return false;}
};
template<class Iterator> inline void CheckJsonBudget(Iterator first,Iterator last){JsonBudget budget;if(!nlohmann::json::sax_parse(first,last,&budget))throw std::runtime_error(budget.Error.empty()?"Invalid JSON":budget.Error);}

} // namespace
