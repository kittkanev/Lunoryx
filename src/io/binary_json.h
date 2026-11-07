#pragma once
// Base64 bounds JSON DOM growth; keep reading legacy numeric byte arrays.
static nlohmann::json SaveBinary(const std::vector<unsigned char>& bytes) {
  static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  if(bytes.size()>16*1024*1024)throw std::runtime_error("Embedded resource exceeds 16 MiB limit");
  std::string out;out.reserve((bytes.size()+2)/3*4);
  for(size_t i=0;i<bytes.size();i+=3){uint32_t value=uint32_t(bytes[i])<<16;if(i+1<bytes.size())value|=uint32_t(bytes[i+1])<<8;if(i+2<bytes.size())value|=bytes[i+2];out+=alphabet[value>>18];out+=alphabet[(value>>12)&63];out+=i+1<bytes.size()?alphabet[(value>>6)&63]:'=';out+=i+2<bytes.size()?alphabet[value&63]:'=';}
  return {{"encoding","base64"},{"base64",out}};
}
static std::vector<unsigned char> ReadBinary(const nlohmann::json& value,size_t limit=16*1024*1024) {
  if(value.is_array())return ReadUnsignedArray<unsigned char>(value,limit);
  if(!value.is_object()||value.size()!=2||value.value("encoding",std::string())!="base64")throw std::runtime_error("Unknown binary encoding");
  auto input=value.at("base64").get<std::string>();
  if(input.size()%4||input.size()/4>(limit+2)/3)throw std::runtime_error("Invalid binary length");
  auto digit=[](char c)->int{if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;if(c=='+')return 62;if(c=='/')return 63;return -1;};
  std::vector<unsigned char> out;out.reserve(input.size()/4*3);
  for(size_t i=0;i<input.size();i+=4){int a=digit(input[i]),b=digit(input[i+1]),c=input[i+2]=='='?0:digit(input[i+2]),d=input[i+3]=='='?0:digit(input[i+3]);bool padC=input[i+2]=='=',padD=input[i+3]=='=';
    if(a<0||b<0||c<0||d<0||(padC&&!padD)||((padC||padD)&&i+4!=input.size())||(padC&&(b&15))||(padD&&!padC&&(c&3)))throw std::runtime_error("Invalid base64 data");
    uint32_t bits=uint32_t(a<<18|b<<12|c<<6|d);out.push_back(static_cast<unsigned char>(bits>>16));if(!padC)out.push_back(static_cast<unsigned char>(bits>>8));if(!padD)out.push_back(static_cast<unsigned char>(bits));}
  if(out.size()>limit)throw std::runtime_error("Embedded resource exceeds budget");
  return out;
}
