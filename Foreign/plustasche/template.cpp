/**
 * @file template.cpp
 * @brief plustache template
 * @author Daniel Schauenberg <d@unwiredcouch.com>
 */

#include "template.hpp"

#include <string>
#include <cctype>
#include <algorithm>
#include <functional>

#include "plustache_types.hpp"
#include "context.hpp"

using namespace Plustache;


/**
 * @brief trim whitespace from the start of a string
 *
 * @param s the string to trim
 *
 * @return s, trimmed
 */
static inline std::string &ltrim(std::string &s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
        std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}

/**
 * @brief trim whitespace from the end of a string
 *
 * @param s the string to trim
 *
 * @return s, trimmed
 */
static inline std::string &rtrim(std::string &s)
{
    s.erase(std::find_if(s.rbegin(), s.rend(),
        std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}

/**
 * @brief trim whitespace from both ends of a string
 *
 * @param s the string to trim
 *
 * @return s, trimmed
 */
static inline std::string &trim(std::string &s)
{
    return ltrim(rtrim(s));
}

/**
 * @brief constructor taking no arguments
 */
template_t::template_t()
{
    // set template path
    template_path = "";
    template_t::compile_data();
}

/**
 * @brief constructor taking the template path as argument
 *
 * @param tmpl_path path to the template directory
 */
template_t::template_t(const std::string& tmpl_path)
{
    template_path = tmpl_path;
    template_t::compile_data();
}

/**
 * @brief function to compile all the basic data like regex, tags, etc.
 */
void template_t::compile_data()
{
    // lookup table for html escape
    escape_lut["&"] = "&amp;";
    escape_lut["<"] = "&lt;";
    escape_lut[">"] = "&gt;";
    escape_lut["\\"] = "&#92;";
    escape_lut["\""] = "&quot;";
    
    // regex for what to escape in a html std::string
    escape_chars.assign("(<|>|\"|\\\\|&)");
    
    // tag delimiters
    otag = "\\{\\{";
    ctag = "\\}\\}";
    update_tags();
}
/**
 * @brief applies the current ctag/otag settings to the tag & section
 *   regex patterns
 */
void template_t::update_tags() {
    // tag and section regex
    tag.assign(otag + "(#|=|&|!|>|\\{)?(.+?)(\\})?" + ctag);
    section.assign(otag + "(\\^|\\#)([^\\}]*)" + ctag +
        "(?:[ \\t]*\\n)?" +  // ignore leading whitespace
        "((?:.|\\s)+?)" +  // section content
        otag + "\\/\\2" + ctag);
}

/**
 * @brief destructor nothing to do here
 */
template_t::~template_t()
{

}

/**
 * @brief method to render tags with a given map
 *
 * @param tmplate template std::string to render
 * @param ctx map of values
 *
 * @return rendered std::string
 */
std::string template_t::render_tags(const std::string& tmplate,
                                    const Context& ctx)
{
    // initialize data
    std::string ret = "";
    std::string rest = "";
    std::string::const_iterator start, end;
    std::match_results<std::string::const_iterator> matches;
    start = tmplate.begin();
    end = tmplate.end();
    // return whole std::string when no tags are found
    if (!regex_search(start, end, matches, tag,
        std::regex_constants::match_default |
        std::regex_constants::format_default))
    {
        ret = tmplate;
    }
    // loop through tags and replace
    while (regex_search(start, end, matches, tag,
        std::regex_constants::match_default |
        std::regex_constants::format_default))
    {
        std::string modifier(matches[1].first, matches[1].second);
        std::string key(matches[2].first, matches[2].second);
        trim(key);
        trim(modifier);
        std::string text(start, matches[0].second);
        std::string repl;
        // don't html escape this
        if (modifier == "&" || modifier == "{")
        {
            try
            {
                // get value
                std::string s = ctx.get(key)[0][key];
                // escape backslash in std::string
                const std::string f = "\\";
                size_t found = s.find(f);
                while(found != std::string::npos)
                {
                    s.replace(found,f.length(),"\\");
                    found = s.find(f, found+2);
                }
                repl.assign(s);
            }
            catch(int) { repl.assign(""); }
        }
        // this is a comment
        else if (modifier == "!")
        {
            repl.assign("");
        }
        // found a partial
        else if (modifier == ">")
        {
            std::string partial = template_t::get_partial(key);
            repl.assign(template_t::render(partial, ctx));
        }
        // normal tag
        else
        {
            try
            {
                repl.assign(template_t::html_escape(ctx.get(key)[0][key]));
            }
            catch(int) { repl.assign(""); }
        }

        // replace
        ret += regex_replace(text, tag, repl,
            std::regex_constants::match_default |
            std::regex_constants::format_default);
        // change delimiter after was removed
        if (modifier == "=")
        {
          // regex for finding delimiters
          std::regex delim("(.+?) (.+?)=");
          // match object
          std::match_results<std::string::const_iterator> delim_m;
          // search for the delimiters
          std::regex_search(matches[2].first, matches[2].second, delim_m, delim,
              std::regex_constants::match_default |
              std::regex_constants::format_default);
          // set new otag and ctag
          std::string new_otag = delim_m[1];
          std::string new_ctag = delim_m[2];
          // change delimiters
          template_t::change_delimiter(new_otag, new_ctag);
        }
        // set start for next tag and rest of std::string
        rest.assign(matches[0].second, end);
        start = matches[0].second;
        
        // Reset for next round (required for libstdc++ implementation)
        matches = std::match_results<std::string::const_iterator>();
    }
    // append and return
    ret += rest;
    return ret;
}

/**
 * @brief method to render sections with a given map
 *
 * @param tmplate template std::string to render
 * @param ctx map of values
 *
 * @return rendered std::string
 */
std::string template_t::render_sections(const std::string& tmplate,
                                        const Context& ctx)
{
    // initialize data structures
    std::string ret = "";
    std::string rest = "";
    std::string::const_iterator start, end;
    std::match_results<std::string::const_iterator> matches;
    start = tmplate.begin();
    end = tmplate.end();
    // return the whole template if no sections are found
    if (!std::regex_search(start, end, matches, section,
        std::regex_constants::match_default |
        std::regex_constants::format_default))
    {
        ret = tmplate;
    }
    // loop through sections and render
    while (std::regex_search(start, end, matches, section,
        std::regex_constants::match_default |
        std::regex_constants::format_default))
    {
        // std::string assignments
        std::string text(start, matches[0].second);
        std::string key(matches[2].first, matches[2].second);
        std::string modifier(matches[1]);
        
        // trimming
        trim(key);
        trim(modifier);
        std::string repl = "";
        std::string show = "false";
        CollectionType values;
        values = ctx.get(key);
        
        if (values.size() == 1)
        {
            // if we don't have a collection, we find the key and an
            // empty map bucket means false
            if (values[0].find(key) != values[0].end())
            {
              show = values[0][key] != "" ? values[0][key] : "false";
            }
            // if we have a collection, we want to show it if there is
            // something to show
            else
            {
              show = values[0].size() > 0 ? "true" : "false";
            }
        }
        else if(values.size() > 1)
        {
            show = "true";
        }
        
        // inverted section?
        if (modifier == "^" && show == "false") show = "true";
        else if (modifier == "^" && show == "true") show = "false";
        
        // Generate content
        if (show == "true")
        {
            for(CollectionType::iterator it = values.begin();
                it != values.end(); ++it)
            {
                std::string content = matches[3];
                Context small_ctx;
                small_ctx = ctx;
                small_ctx.add(*it);
                
                if (std::regex_search(content, section,
                    std::regex_constants::match_default |
                    std::regex_constants::format_default))
                {
                    content.assign(template_t::render_sections(
                        content, small_ctx));
                }
                
                repl += template_t::render_tags(content, small_ctx);
            }
        }
        // Hide section with empty content
        else {
            repl.assign("");
        }
        
        // Replace matched section with generated text
        ret += std::regex_replace(text, section, repl,
            std::regex_constants::match_default |
            std::regex_constants::format_default);
        
        // Check on end-of-section newlines
        bool shouldSkipNextNewline =
            // If the section ended in a newline, or was empty...
            ((repl[repl.length() - 1] == '\n') || show == "false") &&
            // ...and the section is also followed by a newline, skip it.
            (std::string(matches[0].second, matches[0].second + 1) == "\n");
        
        // Store the rest of the template for the next pass
        auto next = matches[0].second + (shouldSkipNextNewline ? 1 : 0);
        rest.assign(next, end);
        start = next;
    }
    
    // append and return
    ret += rest;
    
    return ret;
}

/**
 * @brief method for rendering a template
 *
 * @param tmplate template to render as raw std::string or file path
 * @param ctx context object
 *
 * @return rendered std::string
 */
std::string template_t::render(const std::string& tmplate, const Context& ctx)
{
    // get template
    std::string tmp = get_template(tmplate);

    std::string first = template_t::render_sections(tmp, ctx);
    std::string second = template_t::render_tags(first, ctx);
    return second;
}

/**
 * @brief method for rendering a template
 *
 * @param tmplate template to render as raw std::string or filepath
 * @param ctx map of values
 *
 * @return rendered std::string
 */
std::string template_t::render(const std::string& tmplate,
                               const ObjectType& ctx)
{
    // get template
    std::string tmp = get_template(tmplate);
    Context contxt;
    contxt.add(ctx);

    std::string first = template_t::render_sections(tmp, contxt);
    std::string second = template_t::render_tags(first, contxt);
    return second;
}

//
// HELPER FUNCTIONS
//


/**
 * @brief method to escape html std::strings
 *
 * @param s std::string to escape
 *
 * @return escaped std::string
 */
std::string template_t::html_escape(const std::string& s)
{
    /** initialize working std::strings and iterators */
    std::string ret = "";
    std::string rest = "";
    std::string::const_iterator start, end;
    std::match_results<std::string::const_iterator> matches;
    start = s.begin();
    end = s.end();
    // return original std::string if nothing is found
    if (!std::regex_search(start, end, matches, escape_chars,
        std::regex_constants::match_default |
        std::regex_constants::format_default))
    {
        ret = s;
    }
    // search for html chars
    while (std::regex_search(start, end, matches, escape_chars,
        std::regex_constants::match_default |
        std::regex_constants::format_default))
    {
        std::string key(matches[0].first, matches[0].second);
        std::string text(start, matches[0].second);
        trim(key);
        std::string repl;
        repl = escape_lut[key];
        ret += std::regex_replace(text, escape_chars, repl,
            std::regex_constants::match_default |
            std::regex_constants::format_default);
        rest.assign(matches[0].second, end);
        start = matches[0].second;
    }
    ret += rest;
    return ret;
}


/**
 * @brief method to load partial template from file
 *
 * @param s name of the partial to load
 *
 * @return partial template as std::string
 */
std::string template_t::get_partial(const std::string& partial) const
{
    std::string ret = "";
    std::string file_with_path = template_path;
    file_with_path += partial;
    file_with_path += ".mustache";
    // file path with template path prefix
    std::ifstream extended_file(file_with_path.c_str());
    // true if it was a valid file path
    if (extended_file.is_open())
    {
        ret.assign((std::istreambuf_iterator<char>(extended_file)),
                    std::istreambuf_iterator<char>());
        extended_file.close();
    }
    else
    {
        // file path without prefix
        std::ifstream file(partial.c_str());
        if(file.is_open())
        {
          ret.assign((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
          file.close();
        }
    }
    return ret;
}

/**
 * @brief method to change delimiters
 *
 * @param opentag delimiter for open tag
 * @param closetag delimiter for closed tag
 */
void template_t::change_delimiter(const std::string& opentag,
                                  const std::string& closetag)
{
    otag = opentag;
    ctag = closetag;
    update_tags();
}

/**
 * @brief function to prepare template for rendering
 *
 * @param tmpl path to template or template directory
 *
 * @return template as std::string
 */
std::string template_t::get_template(const std::string& tmpl)
{
    // std::string to hold the template
    std::string tmp = "";
    std::ifstream file(tmpl.c_str());
    std::ifstream file_from_tmpl_dir((template_path + tmpl).c_str());
    // true if it was a valid local file path
    if (file.is_open())
    {
        tmp.assign((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
        file.close();
    }
    // maybe the template is in the standard directory
    else if (file_from_tmpl_dir.is_open())
    {
        tmp.assign((std::istreambuf_iterator<char>(file_from_tmpl_dir)),
                    std::istreambuf_iterator<char>());
        file_from_tmpl_dir.close();

    }
    // tmplate was maybe a complete template and no file path
    else
    {
        tmp = tmpl;
    }

    // cleanup
    if (file.is_open()) file.close();
    if (file_from_tmpl_dir.is_open()) file_from_tmpl_dir.close();

    // return template
    return tmp;
}
