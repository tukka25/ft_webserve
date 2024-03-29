

#pragma once
#ifndef BASECONFIG_HPP
# define BASECONFIG_HPP


#include "../parsing/ContextNode.hpp"
#include "../parsing/DirectiveNode.hpp"
#include "TryFilesDirective.hpp"
#include "ReturnDirective.hpp"

#include <sstream>


class BaseConfig
{

private:

	bool					isValidAutoindex(const std::string &autoindexValue);
	void					processFallbackStatusCode(const std::string &statusCode);
	void					splitValueAndUnit(const std::string &bodySize, std::string &value, std::string &unit);
	size_t					safeStringToSizeT(const std::string &bodySize, const std::string value);

public:
	std::string								root;
	std::vector<std::string>				index;
	std::string								autoindex;
	std::map<int, std::string>				errorPages;
	std::map<int, std::string>				errorPagesContext;
	size_t									clientMaxBodySize;
	TryFilesDirective						tryFiles;
	ReturnDirective							returnDirective;

	void					setRoot(const std::string &rootValue);
	void					setIndex(const std::vector<std::string> &indexValues);
	void					setAutoindex(const std::string &autoindexValue);
	void					setErrorPage(const std::string &statusCode, const std::string &uri, const std::string &context);
	void					setErrorPage(const std::vector<std::string> &errorPageValues, const std::string &context);
	void					setClientMaxBodySize(const std::string &bodySize);
	void					setTryFiles(const std::vector<std::string> &tryFilesValues);
	void					setReturn(const std::vector<std::string> &returnValues);

};


#endif /* BASECONFIG_HPP */

