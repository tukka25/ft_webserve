

#pragma once
#ifndef LOGICVALIDATOR_HPP
#define LOGICVALIDATOR_HPP


#include "ContextNode.hpp"
#include "DirectiveNode.hpp"



enum ParentPresence
{
    ParentNeeded,
    Independent
};

enum numArgs
{
    OneArg,
    TwoArgs,
	OneOrMoreArgs,
	TwoOrMoreArgs,
	OneOrTwoArgs
};


class   LogicValidator 
{
    private:
        std::map<std::string, std::pair<int , int> >  possibleDirs;
    public:
        LogicValidator();
        void    validateDirectives(ConfigNode *node);
        void	validateDirectiveCodes(DirectiveNode *directiveNode);
        void	validateDirectiveParent(const std::string &key, const std::string &parentName);
		void	validateDirectiveArgs(DirectiveNode *directive, std::map<std::string, std::pair<int, int> >::iterator it);
        ~LogicValidator();
};


#endif /* LOGICVALIDATOR_HPP */