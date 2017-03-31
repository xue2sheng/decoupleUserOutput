#include <boost/hana.hpp>
#include <string>
#include <fstream>
#include <vector>
#include <utility>
#include <map>
#include <set>
#include <functional>
#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/pointer.h>

// automatically generate version info
#include "version.h"
#include "boilerplateCodeDoc.h"

/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/
/*********************** BOILERPLATE CODE ***********************************************/
/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

static inline std::string to_string(const boilerplateCodeDoc::ParseErrorCode& error) {
	    switch(error) {
		default: return "Unknown ParseErrorCode";
		case boilerplateCodeDoc::ParseErrorCode::OK: return "OK";
		case boilerplateCodeDoc::ParseErrorCode::EMPTY_FILENAME: return "EMPTY_FILENAME";
		case boilerplateCodeDoc::ParseErrorCode::UNEXPECTED_ERROR_PROCESSING_SCHEMA_JSON_FILE: return "UNEXPECTED_ERROR_PROCESSING_SCHEMA_JSON_FILE";
		case boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON: return "ERROR_PARSING_SCHEMA_JSON";
		case boilerplateCodeDoc::ParseErrorCode::UNABLE_OPEN_FILE: return "UNABLE_OPEN_FILE";
	    }
}

boilerplateCodeDoc::JsonSchema::~JsonSchema()
{
	if(document_ptr) {
		delete reinterpret_cast<rapidjson::Document*>(document_ptr);
		document_ptr = nullptr;
	}
}

boilerplateCodeDoc::JsonSchema::JsonSchema(std::string filename)
{

	if( filename.empty() ) {
		error = boilerplateCodeDoc::ParseErrorCode::EMPTY_FILENAME;
		message = to_string(error);
		return;
	}

	try {

		std::ifstream json{filename};
		if( json.is_open() ) {

		     // temporary var
		     std::string contents;

		     // allocate all the memory up front
		     json.seekg(0, std::ios::end);
		     contents.reserve(json.tellg());
		     json.seekg(0, std::ios::beg);

		     // read data
		     contents.assign((std::istreambuf_iterator<char>(json)), std::istreambuf_iterator<char>());

		     // get json ready to process
		     rapidjson::Document* temp = new rapidjson::Document;
		     rapidjson::Document& document {*temp};
		     document_ptr = reinterpret_cast<void*>(temp);

		     rapidjson::ParseResult ok = document.Parse(contents.c_str());
		     if( not ok ) {
			 error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
			 message = to_string(error);
			 message += " [";
			 message += std::string(rapidjson::GetParseError_En(ok.Code())) + " : " + std::to_string(ok.Offset());
			 message += "]";
			 return;
		     }

		     if( document.IsNull() ) {
			     error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
			     message = "Json Schema document shouldn't be NULL";
			     return;
		     }

		     if(document.IsObject()) {
			     if( not document.HasMember("$schema") ) {
				error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
				message = "Missing expected $schema root object";
				return;
			     }
			     if( not document.HasMember("title") ) {
				error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
				message = "Missing expected schema title";
				return;
			     }

			     if( not document.HasMember("description") ) {
				error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
				message = "Missing expected schema description";
				return;
			     }

			     title = document["title"].GetString();
			     description = document["description"].GetString();

			     if( document.HasMember("namespace") ) {
				      namespace_id = document["namespace"].GetString();
			      }
			     if( document.HasMember("cssClass") ) {
				      css_class = document["cssClass"].GetString();
			      }
			     if( document.HasMember("cppFileName") ) {
				      cpp_filename = document["cppFileName"].GetString();
			      }

			     error = boilerplateCodeDoc::ParseErrorCode::OK;
			     message = to_string(error);

		     } else {
			error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
			message = "Root element of a schema json should be an object";
		     }

		} else {
			error = boilerplateCodeDoc::ParseErrorCode::UNABLE_OPEN_FILE;
			message = to_string(error);
		}

	} catch(...) {
		error = boilerplateCodeDoc::ParseErrorCode::UNEXPECTED_ERROR_PROCESSING_SCHEMA_JSON_FILE;
		message = to_string(error);
	}
}

static inline bool ignore(const std::string& name, const std::set<std::string>& ignoreSet) {
	    return ignoreSet.end() !=  ignoreSet.find(name);
}
using Required = std::vector<std::string>;
using OneOf = std::vector<Required>;
struct Property {
    bool required {false};
    std::string scope {};
    std::string name {};
    std::string type {};
    std::string description {};
    std::string title {};
    std::string parentTitle {};
    std::string metatype {};
    std::string parentMetatype {};
    std::string metainfo {};
    std::string bookmark_source {};
    std::string bookmark_target {};
    Property(bool req, std::string sco, std::string nam, std::string typ, std::string des, std::string tit,
	     std::string par, std::string met, std::string pm, std::string inf, std::string bs, std::string bt)
	: required{req}, scope{std::move(sco)}, name{std::move(nam)}, type{std::move(typ)}, description{std::move(des)}, title{std::move(tit)},
	  parentTitle{std::move(par)}, metatype{std::move(met)}, parentMetatype{std::move(pm)}, metainfo{std::move(inf)},
	  bookmark_source{std::move(bs)}, bookmark_target{std::move(bt)} {}
};
using Properties = std::map<std::string, Property>;

static bool getString(const rapidjson::Document& document, const std::string& element, const std::string& prefix, const std::string& name, const std::string& suffix, std::string& result)
{
  // Do nothing if there's nothing to do
  std::string temp {element + prefix + name + suffix};
  if( not rapidjson::Pointer(temp.c_str()).IsValid() ) { return false; }
  const rapidjson::Value* pointer {rapidjson::Pointer(temp.c_str()).Get(document)};
  if( not pointer ) { return false; }
  if( pointer->IsNull() ) { return false; }
  if( not pointer->IsString() ) { return false; }
  result = pointer->GetString();
  return true;
}

static void processProperties(const OneOf& oneOf, const Required& required, Properties& properties)
{
	if( oneOf.size() > 0 ) {

        // tricky part of selecting which "oneOf" array of requirements must be applied
        // because depends on the json document to be validated by this json schema
        // therefore all posibilites will be mark as required and their 'descriptions' are supposed to provided extra details
		for(const auto& o : oneOf) {
            for(const auto& r : o) {
                 auto found = properties.find(r);
                 if( found != properties.end() ) {
		     found->second.required = true;

		     // if nothing was explicitly stated, "required" will be used as "scope"
		     if( found->second.scope.empty() ) { found->second.scope = "required"; }

		 }
            }
		}

	} else if ( required.size() > 0 ) {
		for(const auto& r : required) {
			auto found = properties.find(r);
			if( found != properties.end() ) {
				found->second.required = true;

				// if nothing was explicitly stated, "required" will be used as "scope"
				if( found->second.scope.empty() ) { found->second.scope = "required"; }
			}
		}
	}

	// by default "scope" will be "optional"
	for(auto&& p : properties) {
		if( p.second.scope.empty() ) { p.second.scope = "optional"; }
	}

}

using lambda_t = std::function<void(const Properties&, std::string&, std::string&)>;

static void SetProperties(const rapidjson::Document& document, std::string element, std::string& filtered, const lambda_t& lambda, std::string& extra)
{
    OneOf oneOf {};
    Required required {};
    Properties properties {};
    std::string nextElement {};

    // Do nothing if there's nothing to do
    if( not rapidjson::Pointer(element.c_str()).IsValid() ) { return; }
    const rapidjson::Value* pointer {rapidjson::Pointer(element.c_str()).Get(document)};
    if( not pointer ) { return; }
    if( pointer->IsNull() ) { return; }
    if( not pointer->IsObject() ) { return; }

    auto&& object {pointer->GetObject()};

    for(rapidjson::Value::ConstMemberIterator i = pointer->GetObject().MemberBegin(); i != pointer->GetObject().MemberEnd(); ++i) {
        std::string name {i->name.GetString()};
        if( name == "oneOf" ) {
            for(auto&& j : object["oneOf"].GetArray()) {
                Required temp {};
                for(auto&& k : j["required"].GetArray()) {
                   temp.emplace_back(std::string{k.GetString()});
                }
                oneOf.emplace_back(temp);
            }
        } else if( name == "required" ) {
            for(auto&& j : object["required"].GetArray()) {
                required.emplace_back(std::string{j.GetString()});
            }
        } else if( name == "properties" ) {
            for(auto&& j = object["properties"].MemberBegin(); j != object["properties"].MemberEnd(); ++j) {
              std::string name{j->name.GetString()};
              std::string type {}; // required
              if(not getString(document, element, "/properties/", name, "/type", type)) { continue; }
              std::string description {}; // optional
              getString(document, element, "/properties/", name, "/description", description);
              std::string title {}; // optional
              getString(document, element, "/properties/", name, "/title", title);
              std::string parentTitle {}; // optional
              getString(document, element, "", "", "/title", parentTitle);
	      std::string parentMetatype {}; // required
	      if(not getString(document, element, "", "", "/metatype", parentMetatype)) { continue; }
	      std::string metatype {}; // required
	      if(not getString(document, element, "/properties/", name, "/metatype", metatype)) { continue; }
	      std::string metainfo {}; // optional
	      getString(document, element, "/properties/", name, "/metainfo", metainfo);
	      std::string scope {}; // optional
	      getString(document, element, "/properties/", name, "/scope", scope);
	      std::string bookmark_source {}; // optional
	      getString(document, element, "/properties/", name, "/bookmarkSource", bookmark_source);
	      std::string bookmark_target {}; // optional
	      getString(document, element, "", "", "/bookmarkTarget", bookmark_target);
	      properties.emplace(std::make_pair(name, Property{false, scope, name, type, description, title,
							       parentTitle, metatype, parentMetatype, metainfo, bookmark_source, bookmark_target}));
            }

            nextElement = element + "/properties/"; // recursive call

        } else if( name == "items" ) {
            if(object["items"].GetObject().HasMember("properties")) {
             auto&& items {object["items"]["properties"].GetObject()};
             for(auto&& j = items.MemberBegin(); j != items.MemberEnd(); ++j) {
              std::string name{j->name.GetString()};
              std::string type {}; // required
              if(not getString(document, element, "/items/properties/", name, "/type", type)) { continue; }
              std::string description {}; // optional
              getString(document, element, "/items/properties/", name, "/description", description);
              std::string title {}; // optional
	      getString(document, element, "/items/properties/", name, "/title", title);
              std::string parentTitle {}; // optional
	      getString(document, element, "/items", "", "/title", parentTitle);
	      std::string parentMetatype {}; // optional
	      getString(document, element, "/items", "", "/metatype", parentMetatype);
	      std::string metatype {}; // optional
	      getString(document, element, "/items/properties/", name, "/metatype", metatype);
	      std::string metainfo {}; // optional
	      getString(document, element, "/items/properties/", name, "/metainfo", metainfo);
	      std::string scope {}; // optional
	      getString(document, element, "/items/properties/", name, "/scope", scope);
	      std::string bookmark_source {}; // optional
	      getString(document, element, "/items/properties/", name, "/bookmarkSource", bookmark_source);
	      std::string bookmark_target {}; // optional
	      getString(document, element, "/items", "", "/bookmarkTarget", bookmark_target);
	      properties.emplace(std::make_pair(name, Property{false, scope, name, type, description, title,
							       parentTitle, metatype, parentMetatype, metainfo, bookmark_source, bookmark_target}));
            }

            nextElement = element + "/items/properties/"; // recursive call

           }
           if(object["items"].GetObject().HasMember("required")) {
            for(auto&& j : object["items"]["required"].GetArray()) {
                required.emplace_back(std::string{j.GetString()});
            }
           }
           if(object["items"].GetObject().HasMember("oneOf")) {
            for(auto&& j : object["items"]["oneOf"].GetArray()) {
                Required temp {};
                for(auto&& k : j["required"].GetArray()) {
                   temp.emplace_back(std::string{k.GetString()});
                }
                oneOf.emplace_back(temp);
            }
           }
        }
    }
    processProperties(oneOf, required, properties); // what is required
    lambda(properties, filtered, extra); // apply filter

    // recursive call
    for(const auto& p : properties) {
        if( "object" == p.second.type || "array" == p.second.type) {
	    SetProperties(document, nextElement + p.second.name, filtered, lambda, extra);
        }
    }

}

static bool boilerplateOperator(const boilerplateCodeDoc::JsonSchema& jsonSchema, boilerplateCodeDoc::JsonSchemaFilter& filter,
				const std::string& extra, const lambda_t& lambda)
{
    if( not jsonSchema.document_ptr ) {
	filter.error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
	filter.message = "Empty document pointer";
	return false;
    }

    rapidjson::Document& document {*reinterpret_cast<rapidjson::Document*>(jsonSchema.document_ptr)};
    try {
	    if(document.IsNull() || not document.IsObject() ) {
		filter.error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
		filter.message = "Root element shouldn't be NULL";
		return false;
	    }

	    //std::string element {"#/properties/imp/items/properties/native"};
	    std::string element {"#"};
	    std::string filter_extra {(extra.empty()?filter.extra:extra)};
	    SetProperties(document, element, filter.filtered, lambda, filter_extra );
	    filter.filtered = filter.header + filter.filtered + filter.footer;

	    filter.error = boilerplateCodeDoc::ParseErrorCode::OK;
	    filter.message = to_string(filter.error);
	    return true;

    } catch(...) {
	filter.error = boilerplateCodeDoc::ParseErrorCode::ERROR_PARSING_SCHEMA_JSON;
	filter.message = "Unexpected exception";
	return false;
    }
}

static bool implemented(const std::string& metainfo)
{
	if( metainfo.empty() ) { return true; } // empty candidates are supposedly implemented

	// Look for exact matches
	static std::set<std::string> NOT_IMPLEMENTED {"not implemented", "Not Implemented", "Not implemented", "NOT IMPLEMENTED"};
	if( NOT_IMPLEMENTED.end() != NOT_IMPLEMENTED.find(metainfo) ) { return false; }

	// Look for partial matches
	for(const auto& s : NOT_IMPLEMENTED) { if( metainfo.find(s) != std::string::npos ) { return false; } }

	// Implemented because we got any hint
	return true;
}

/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/
/*********************** ACTUAL LOGIC CODE **********************************************/
/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

bool boilerplateCodeDoc::JsonSchema2HTML::operator()(const boilerplateCodeDoc::JsonSchema& jsonSchema)
{
return boilerplateOperator(jsonSchema, *this, jsonSchema.css_class, [](const Properties& properties, std::string& filtered, std::string& css_class) {

  if(properties.size() > 0) {

    // header
    std::string parentTitle {};
    std::string bookmark_target {};
    for(const auto& p : properties) {
	if( not p.second.parentTitle.empty() ) { parentTitle = p.second.parentTitle; }
	if( not p.second.bookmark_target.empty() ) { bookmark_target = p.second.bookmark_target; }
	if( not p.second.parentTitle.empty() && not p.second.bookmark_target.empty() ) { break; }
    }
    if(not parentTitle.empty() || not bookmark_target.empty()) {
	filtered += "<h3 id=\"" + bookmark_target + "\">" + parentTitle + "</h3>\n";
    }
    filtered += "<table class=\"" + css_class + "\">\n";
    filtered += "<tr><th>Field<th>Scope</th><th>Type</th><th>Description</th><th>Info</th></tr>\n";

    // body
    for(const auto& p : properties) {
	    filtered += "<tr>";
	    if( p.second.bookmark_source.empty() ) {
		 filtered += "<td>" + p.second.name + "</td>";
	    } else {
		 filtered += "<td><a href=\"" + p.second.bookmark_source + "\">" + p.second.name + "</a></td>";
	    }
	    filtered += "<td>" + p.second.scope + "</td>";
	    filtered += "<td>" + p.second.type + "</td>";
	    filtered += "<td>" + p.second.description + "</td>";
	    filtered += "<td>" + p.second.metainfo + "</td>";
	    filtered += "</tr>\n";
    }

    if( properties.size() > 0) {
	filtered += "</table>\n<br /><br />\n";
    }
  }

}); // return boilerplateOperator
} // operator()

/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/
/*********************** ACTUAL LOGIC CODE **********************************************/
/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

bool boilerplateCodeDoc::JsonSchema2H::operator()(const boilerplateCodeDoc::JsonSchema& jsonSchema)
{
if( not jsonSchema.cpp_filename.empty() && not header.empty() ) { header = "/** @file " + jsonSchema.cpp_filename + ".h" + header; }
return boilerplateOperator(jsonSchema, *this, jsonSchema.namespace_id, [](const Properties& properties, std::string& filtered, std::string& namespace_id) {

  if(properties.size() > 0) {

    // supposed metatype is a must
    std::string parentMetatype {};
    parentMetatype = properties.begin()->second.parentMetatype;
    if( parentMetatype.empty() ) { return; } // required

    // if nothing is implemented, do nothing
    bool nothing_implemented {true};
    for(const auto& p : properties) {
	if( implemented(p.second.metainfo) ) { nothing_implemented = false; break; }
    }
    if( nothing_implemented ) {
	    filtered = "\n// " + parentMetatype + ": all their properties are not implemented\n\n" + filtered;
	    return;
    }

    std::string addition {};
    if( not namespace_id.empty() ) { addition += "namespace " + namespace_id + " {\n"; }
    addition += "\n" + parentMetatype + " {\n\n";

    for(const auto& p : properties) {

	    if( not implemented(p.second.metainfo) ) { addition += "// " + p.second.name + ": " + p.second.metainfo + "\n"; continue; }

	    std::string metatype {p.second.metatype};
	    if( metatype.empty() ) { continue; } // required
	    std::string name {p.second.name};
	    if( name.empty() ) { continue; } // required

	    if( not p.second.description.empty() ) { addition += "///@ brief " + p.second.description + "\n"; }
	    addition += metatype + " " + name + " {};\n";
    }

    addition += "\n}; // " + parentMetatype + "\n";
    if( not namespace_id.empty() ) { addition += "\n} // namespace " + namespace_id + "\n\n"; }

    filtered = addition + filtered;
  }

}); // return boilerplateOperator
} // operator()

/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/
/*********************** ACTUAL LOGIC CODE **********************************************/
/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

bool boilerplateCodeDoc::JsonSchema2CPP::operator()(const boilerplateCodeDoc::JsonSchema& jsonSchema)
{
if( not jsonSchema.cpp_filename.empty() && not header.empty() ) { header = "/** @file " + jsonSchema.cpp_filename + ".cpp" + header; }
return boilerplateOperator(jsonSchema, *this, jsonSchema.namespace_id, [](const Properties& properties, std::string& filtered, std::string& namespace_id) {

	/*
  if(properties.size() > 0) {

    // supposed metatype is a must
    std::string parentMetatype {};
    parentMetatype = properties.begin()->second.parentMetatype;
    if( parentMetatype.empty() ) { return; } // required

    // if nothing is implemented, do nothing
    bool nothing_implemented {true};
    for(const auto& p : properties) {
	if( implemented(p.second.metainfo) ) { nothing_implemented = false; break; }
    }
    if( nothing_implemented ) {
	    filtered = "\n// " + parentMetatype + ": all their properties are not implemented\n\n" + filtered;
	    return;
    }

    std::string addition {};
    if( not namespace_id.empty() ) { addition += "namespace " + namespace_id + " {\n"; }
    addition += "\n" + parentMetatype + " {\n\n";

    for(const auto& p : properties) {

	    if( not implemented(p.second.metainfo) ) { addition += "// " + p.second.name + ": " + p.second.metainfo + "\n"; continue; }

	    std::string metatype {p.second.metatype};
	    if( metatype.empty() ) { continue; } // required
	    std::string name {p.second.name};
	    if( name.empty() ) { continue; } // required

	    if( not p.second.description.empty() ) { addition += "///@ brief " + p.second.description + "\n"; }
	    addition += metatype + " " + name + " {};\n";
    }

    addition += "\n}; // " + parentMetatype + "\n";
    if( not namespace_id.empty() ) { addition += "\n} // namespace " + namespace_id + "\n\n"; }

    filtered = addition + filtered;
  }
*/
}); // return boilerplateOperator
} // operator()
