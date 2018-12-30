#ifndef MW_STRINGUTIL_H
#define MW_STRINGUTIL_H

#include <string>

namespace MWSTRINGUTIL
{

class StringUtil
{
public:
	static void Split(const std::string& str,
	                  const std::string& delim,
	                  std::vector<std::string>* result);

	// �ж��ַ���str�Ƿ�����prefix��ͷ
	static bool StartsWith(const std::string& str, const std::string& prefix);
	static bool EndsWith(const std::string& str, const std::string& suffix);

	static std::string& Ltrim(std::string& str); // NOLINT
	static std::string& Rtrim(std::string& str); // NOLINT
	static std::string& Trim(std::string& str);  // NOLINT

	static void Trim(std::vector<std::string>* strList);

	// �Ӵ��滻
	static void StringReplace(const std::string& subStr1, const std::string& subStr2, std::string* str);

	static void UrlEncode(const std::string& srcStr, std::string* dstStr);
	static void UrlDecode(const std::string& srcStr, std::string* dstStr);

	// ��Сдת��
	static void ToUpper(std::string* str);
	static void ToLower(std::string* str);

	// ȥͷȥβ
	static bool StripSuffix(std::string* str, const std::string& suffix);
	static bool StripPrefix(std::string* str, const std::string& prefix);

	// bin��hexת��
	static bool Hex2Bin(const char* hexStr, std::string* binStr);
	static bool Bin2Hex(const char* binStr, size_t binLen, std::string* hexStr);	
	static void ToHexString(const std::string& src, std::string& dest);
	static const char* ToHexString(const std::string& src);
	static const char* ToHexString(const char* src, size_t len);
};

} // end namespace
#endif
