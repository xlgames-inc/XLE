/**
 * @file template.cpp
 * @brief plustache template
 * @author Daniel Schauenberg <d@unwiredcouch.com>
 */

////////////////////////////////////////////////////////////////////////////////////
// DavidJ -- note -- this file has been modified to remove references to boost
////////////////////////////////////////////////////////////////////////////////////

#include <plustache/template.hpp>

#include <string>
#include <regex>
#include <cctype>

#include <plustache/plustache_types.hpp>
#include <plustache/context.hpp>

using namespace Plustache;

static void trim(std::string& str)
{
	auto i = str.begin();
	while (i != str.end() && std::isspace((int)*i)) ++i;
	auto i2 = str.end();
	while (i2 != str.begin() && std::isspace((int)*(i2-1))) --i2;
	if (i != str.begin() || i2 != str.end())
		str = std::string(i, i2);
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
    otag = "\\{\\{";
    ctag = "\\}\\}";
    // tag and section regex
    tag.assign(otag + "(#|=|&|!|>|\\{)?(.+?)(\\})?" + ctag);
    section.assign(otag + "(\\^|\\#)([^\\}]*)" + ctag + "\\s*(.+?)\\s*"
                   + otag + "/\\2"+ctag);
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
    if (!std::regex_search(start, end, matches, tag, std::regex_constants::match_default | std::regex_constants::format_default))
    {
        ret = tmplate;
    }
    // loop through tags and replace
    while (regex_search(start, end, matches, tag, std::regex_constants::match_default | std::regex_constants::format_default))
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
                repl.assign(ctx.get(key)[0][key]);
            }
			catch (int i) { repl.assign(""); (void)i; }
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
			catch (int i) { repl.assign(""); (void)i; }
        }

        // replace
        ret += std::regex_replace(text, tag, repl, std::regex_constants::match_default | std::regex_constants::format_default);
        // change delimiter after was removed
        if (modifier == "=")
        {
          // regex for finding delimiters
          std::regex delim("(.+?) (.+?)=");
          // match object
          std::match_results<std::string::const_iterator> delim_m;
          // search for the delimiters
          std::regex_search(matches[2].first, matches[2].second, delim_m, delim, std::regex_constants::match_default | std::regex_constants::format_default);
          // set new otag and ctag
          std::string new_otag = delim_m[1];
          std::string new_ctag = delim_m[2];
          // change delimiters
          template_t::change_delimiter(new_otag, new_ctag);
        }
        // set start for next tag and rest of std::string
        rest.assign(matches[0].second, end);
        start = matches[0].second;
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
    if (!std::regex_search(start, end, matches, section, std::regex_constants::match_default | std::regex_constants::format_default))
    {
        ret = tmplate;
    }
    // loop through sections and render
    while (std::regex_search(start, end, matches, section, std::regex_constants::match_default | std::regex_constants::format_default))
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
        // assign replacement content
        if (show == "true")
        {
            if (std::regex_search(matches[3].first, matches[3].second, section, std::regex_constants::match_default | std::regex_constants::format_default))
            {
                repl.assign(template_t::render_sections(matches[3], ctx));
            }
            else
            {
                for(CollectionType::iterator it = values.begin();
                    it != values.end(); ++it)
                {
                  Context small_ctx;
                  small_ctx = ctx;
                  small_ctx.add(*it);
                  repl += template_t::render_tags(matches[3], small_ctx);
                }
            }
        }
        else repl.assign("");
        ret += std::regex_replace(text, section, repl, std::regex_constants::match_default | std::regex_constants::format_default);
        rest.assign(matches[0].second, end);
        start = matches[0].second;
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
    if (!std::regex_search(start, end, matches, escape_chars, std::regex_constants::match_default | std::regex_constants::format_default))
    {
        ret = s;
    }
    // search for html chars
    while (std::regex_search(start, end, matches, escape_chars, std::regex_constants::match_default | std::regex_constants::format_default))
    {
        std::string key(matches[0].first, matches[0].second);
        std::string text(start, matches[0].second);
        trim(key);
        std::string repl;
        repl = escape_lut[key];
        ret += std::regex_replace(text, escape_chars, repl, std::regex_constants::match_default | std::regex_constants::format_default);
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
    // tag and section regex
    template_t::tag.assign(otag + "(#|=|&|!|>|\\{)?(.+?)(\\})?" + ctag);
    template_t::section.assign(otag + "(\\^|\\#)([^\\}]*)" + ctag +
                               "\\s*(.+?)\\s*" + otag + "/\\2"+ctag);
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
