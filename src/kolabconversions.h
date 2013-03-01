/*
 * Copyright (C) 2012  Christian Mollekopf <mollekopf@kolabsys.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KOLABOBJECTCONVERSION_H
#define KOLABOBJECTCONVERSION_H

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <XMLParserWrapper.h>

#include <bindings/kolabformat.hxx>
#include <bindings/kolabformat-xcard.hxx>

#include "kolabcontainers.h"
#include "global_definitions.h"
#include "utils.h"
#include "kolabnote.h"
#include "shared_conversions.h"
#include "kolabconfiguration.h"
#include "kolabfile.h"
#include "base64.h"

namespace Kolab {
    namespace KolabObjects {
        
const char* const KOLAB_NAMESPACE = "http://kolab.org";
const char* const BASE64 = "BASE64";
    
using namespace Kolab::Utils;
using namespace Kolab::Shared;


Kolab::Attachment toAttachment(KolabXSD::attachmentPropType aProp)
{
    Kolab::Attachment a;
    const KolabXSD::attachmentPropType ::parameters_type &parameters = aProp.parameters();
    std::string mimetype = parameters.fmttype();
    if (parameters.encoding() && (*parameters.encoding() != BASE64)) {
        ERROR("wrong encoding");
        return Kolab::Attachment();
    }
    if (parameters.x_label()) {
        a.setLabel(*parameters.x_label());
    }
    if (mimetype.empty()) {
        ERROR("no mimetype");
    }

    if (aProp.uri()) {
        a.setUri(*aProp.uri(), mimetype);
    } else if (aProp.binary()) {
        a.setData(base64_decode(*aProp.binary()), mimetype);
    } else {
        ERROR("not uri and no data available");
    }
    return a;
}

KolabXSD::attachmentPropType fromAttachment(const Kolab::Attachment &a)
{
    KolabXSD::attachmentPropType::parameters_type p(a.mimetype());
    if (!a.label().empty()) {
        p.x_label(a.label());
    }
    if (!a.data().empty()) {
        p.encoding(BASE64);
    }

    KolabXSD::attachmentPropType attachment(p);
    if (!a.uri().empty()) {
        attachment.uri(a.uri());
    } else  if (!a.data().empty()) {
        attachment.binary(base64_encode(reinterpret_cast<const unsigned char*>(a.data().c_str()), static_cast<unsigned int>(a.data().length())));
    } else {
        ERROR("no uri and no data");
    }
    return attachment;
}


void writeColors(KolabXSD::Configuration::categorycolor_sequence &colors, const std::vector<CategoryColor> &input)
{
    BOOST_FOREACH (const CategoryColor &entry, input) {
        KolabXSD::Configuration::categorycolor_type c(entry.category());
        c.color(entry.color());
        writeColors(c.categorycolor(), entry.subcategories());
        colors.push_back(c);
    }
}

std::vector<CategoryColor> readColors(const KolabXSD::Configuration::categorycolor_sequence &list)
{
    std::vector<CategoryColor> colors ;
    BOOST_FOREACH (const KolabXSD::Configuration::categorycolor_type &entry, list) {
        if (!entry.color()) {
            ERROR("Color is missing");
            continue;
        }
        CategoryColor color(entry.category());
        color.setColor(*entry.color());
        color.setSubcategories(readColors(entry.categorycolor()));
        colors.push_back(color);
    }
    return colors;
}

KolabXSD::Configuration::type_type getConfiguratinoType(Kolab::Configuration::ConfigurationType t)
{
    switch (t) {
        case Kolab::Configuration::TypeDictionary:
            return KolabXSD::Configuration::type_type::dictionary;
        case Kolab::Configuration::TypeCategoryColor:
            return KolabXSD::Configuration::type_type::categorycolor;
        default:
            CRITICAL("Invalid configuration type");
    }
    return KolabXSD::Configuration::type_type::dictionary;
}


template <typename T>
std::string serializeObject(const T &, const std::string prod = std::string());

template <>
std::string serializeObject <Kolab::Configuration> (const Kolab::Configuration &note, const std::string prod)
{
    clearErrors();
    try {
        const std::string &uid = getUID(note.uid());
        setCreatedUid(uid);

        KolabXSD::Configuration::creation_date_type created(0,0,0,0,0,0);
        if (note.created().isValid()) {
            created = fromDateTime(note.created());
        } else {
            created = fromDateTime(timestamp());
        }
        KolabXSD::Configuration::last_modification_date_type lastModificationDate(0,0,0,0,0,0);
        if (note.lastModified().isValid()) {
            lastModificationDate = fromDateTime(note.lastModified());
        } else {
//             WARNING("missing last_modification_date, fallback to current timestamp");
            lastModificationDate = fromDateTime(timestamp());
        }
        KolabXSD::Configuration n(uid, getProductId(prod), created, lastModificationDate, getConfiguratinoType(note.type()));

        switch (note.type()) {
            case Kolab::Configuration::TypeDictionary: {
                const Kolab::Dictionary &dict = note.dictionary();
                n.language(dict.language());
                BOOST_FOREACH(const std::string &e, dict.entries()) {
                    n.e().push_back(e);
                }
            }
                break;
            case Kolab::Configuration::TypeCategoryColor:
                writeColors(n.categorycolor(), note.categoryColor());
                break;
            default:
                CRITICAL("Invalid configuration type");
                return std::string();
        }

        xml_schema::namespace_infomap map;
        map[""].name = KOLAB_NAMESPACE;

        std::ostringstream ostringstream;
        KolabXSD::configuration(ostringstream, n, map);
        return ostringstream.str();
    } catch  (const xml_schema::exception& e) {
        std::cerr <<  e << std::endl;
    } catch (...) {
        CRITICAL("Unhandled exception");
    }
    CRITICAL("Failed to write configuration!");
    return std::string();
}

template <>
std::string serializeObject <Kolab::Note> (const Kolab::Note &note, const std::string prod)
{
    clearErrors();
    try {
        const std::string &uid = getUID(note.uid());
        setCreatedUid(uid);
        
        KolabXSD::Note::creation_date_type created(0,0,0,0,0,0);
        if (note.created().isValid()) {
            created = fromDateTime(note.created());
        } else {
            created = fromDateTime(timestamp());
        }
        KolabXSD::Note::last_modification_date_type lastModificationDate(0,0,0,0,0,0);
        if (note.lastModified().isValid()) {
            lastModificationDate = fromDateTime(note.lastModified());
        } else {
//             WARNING("missing last_modification_date, fallback to current timestamp");
            lastModificationDate = fromDateTime(timestamp());
        }

        KolabXSD::Note n(uid, getProductId(prod), created, lastModificationDate);

        if (!note.categories().empty()) {
            KolabXSD::Note::categories_sequence categories;
            const std::vector<std::string> &l = note.categories();
            BOOST_FOREACH(const std::string &c, l) {
                categories.push_back(c);
            }
            n.categories(categories);
        }
        switch (note.classification()) {
            case Kolab::ClassPublic:
                n.classification(KolabXSD::Note::classification_type::PUBLIC);
                break;
            case Kolab::ClassPrivate:
                n.classification(KolabXSD::Note::classification_type::PRIVATE);
                break;
            case Kolab::ClassConfidential:
                n.classification(KolabXSD::Note::classification_type::CONFIDENTIAL);
                break;
            default:
                ERROR("unknown classification");
        }
        
        if (!note.attachments().empty()) {
            const std::vector<Kolab::Attachment> &l = note.attachments();
            BOOST_FOREACH(const Kolab::Attachment &a, l) {
                n.attachment().push_back(fromAttachment(a));
            }
        }
        n.summary(note.summary());
        n.description(note.description());
        n.color(note.color());

        if (!note.customProperties().empty()) {
            const std::vector<Kolab::CustomProperty> &l = note.customProperties();
            BOOST_FOREACH(const Kolab::CustomProperty &a, l) {
                n.x_custom().push_back(KolabXSD::CustomType(a.identifier, a.value));
            }
        }

        xml_schema::namespace_infomap map;
        map[""].name = KOLAB_NAMESPACE;

        std::ostringstream ostringstream;
        KolabXSD::note(ostringstream, n, map);
        return ostringstream.str();
    } catch  (const xml_schema::exception& e) {
        std::cerr <<  e << std::endl;
    } catch (...) {
        CRITICAL("Unhandled exception");
    }
    CRITICAL("Failed to write note!");
    return std::string();
}

template <>
std::string serializeObject <Kolab::File> (const Kolab::File &file, const std::string prod)
{
    clearErrors();
    try {
        const std::string &uid = getUID(file.uid());
        setCreatedUid(uid);
        
        KolabXSD::File::creation_date_type created(0,0,0,0,0,0);
        if (file.created().isValid()) {
            created = fromDateTime(file.created());
        } else {
            created = fromDateTime(timestamp());
        }
        KolabXSD::File::last_modification_date_type lastModificationDate(0,0,0,0,0,0);
        if (file.lastModified().isValid()) {
            lastModificationDate = fromDateTime(file.lastModified());
        } else {
//             WARNING("missing last_modification_date, fallback to current timestamp");
            lastModificationDate = fromDateTime(timestamp());
        }
        if (file.file().label().empty()) {
            ERROR("missing filename");
        }

        KolabXSD::File n(uid, getProductId(prod), created, lastModificationDate, fromAttachment(file.file()));

        if (!file.categories().empty()) {
            KolabXSD::File::categories_sequence categories;
            const std::vector<std::string> &l = file.categories();
            BOOST_FOREACH(const std::string &c, l) {
                categories.push_back(c);
            }
            n.categories(categories);
        }
        switch (file.classification()) {
            case Kolab::ClassPublic:
                n.classification(KolabXSD::File::classification_type::PUBLIC);
                break;
            case Kolab::ClassPrivate:
                n.classification(KolabXSD::File::classification_type::PRIVATE);
                break;
            case Kolab::ClassConfidential:
                n.classification(KolabXSD::File::classification_type::CONFIDENTIAL);
                break;
            default:
                ERROR("unknown classification");
        }
        
        n.note(file.note());

        if (!file.customProperties().empty()) {
            const std::vector<Kolab::CustomProperty> &l = file.customProperties();
            BOOST_FOREACH(const Kolab::CustomProperty &a, l) {
                n.x_custom().push_back(KolabXSD::CustomType(a.identifier, a.value));
            }
        }

        xml_schema::namespace_infomap map;
        map[""].name = KOLAB_NAMESPACE;

        std::ostringstream ostringstream;
        KolabXSD::file(ostringstream, n, map);
        return ostringstream.str();
    } catch  (const xml_schema::exception& e) {
        std::cerr <<  e << std::endl;
    } catch (...) {
        CRITICAL("Unhandled exception");
    }
    CRITICAL("Failed to write file!");
    return std::string();
}

template <typename T>
boost::shared_ptr<T> deserializeObject(const std::string& s, bool isUrl);

template <>
boost::shared_ptr<Kolab::Note> deserializeObject <Kolab::Note> (const std::string& s, bool isUrl)
{
    clearErrors();
    try {
        std::auto_ptr<KolabXSD::Note> note;
        if (isUrl) {
            xsd::cxx::xml::dom::auto_ptr <xercesc::DOMDocument > doc = XMLParserWrapper::inst().parseFile(s);
            if (doc.get()) {
                note = KolabXSD::note(doc);
            }
        } else {
            xsd::cxx::xml::dom::auto_ptr <xercesc::DOMDocument > doc = XMLParserWrapper::inst().parseString(s);
            if (doc.get()) {
                note = KolabXSD::note(doc);
            }
        }

        if (!note.get()) {
            CRITICAL("failed to parse note!");
            return boost::shared_ptr<Kolab::Note>();
        }

        boost::shared_ptr<Kolab::Note> n = boost::shared_ptr<Kolab::Note>(new Kolab::Note);
        n->setUid(note->uid());
        n->setCreated(*toDate(note->creation_date()));
        n->setLastModified(*toDate(note->last_modification_date()));
        std::vector<std::string> categories;
        std::copy(note->categories().begin(), note->categories().end(), std::back_inserter(categories));
        n->setCategories(categories);
        if (note->classification()) {
            switch (*note->classification()) {
                case KolabXSD::Note::classification_type::PUBLIC:
                    n->setClassification(Kolab::ClassPublic);
                    break;
                case KolabXSD::Note::classification_type::PRIVATE:
                    n->setClassification(Kolab::ClassPrivate);
                    break;
                case KolabXSD::Note::classification_type::CONFIDENTIAL:
                    n->setClassification(Kolab::ClassConfidential);
                    break;
                default:
                    ERROR("unknown classification");
            }
        }

        if (!note->attachment().empty()) {
            std::vector<Kolab::Attachment> attachments;
            BOOST_FOREACH(KolabXSD::Note::attachment_type &aProp, note->attachment()) {
                const Kolab::Attachment &a = toAttachment(aProp);
                if (!a.isValid()) {
                    ERROR("invalid attachment");
                    continue;
                }
                attachments.push_back(a);
            }
            n->setAttachments(attachments);
        }
        
        if (note->summary()) {
            n->setSummary(*note->summary());
        }
        if (note->description()) {
            n->setDescription(*note->description());
        }
        if (note->color()) {
            n->setColor(*note->color());
        }
        
        setProductId( note->prodid() );
        //         setFormatVersion( vcards->vcard().version().text() );
        //         global_xCardVersion = vcalendar.properties().version().text();
        setKolabVersion( note->version() );
        
        if (!note->x_custom().empty()) {
            std::vector<Kolab::CustomProperty> customProperties;
            BOOST_FOREACH(const KolabXSD::CustomType &p, note->x_custom()) {
                customProperties.push_back(CustomProperty(p.identifier(), p.value()));
            }
            n->setCustomProperties(customProperties);
        }
        return n;
    } catch  (const xml_schema::exception& e) {
        std::cerr <<  e << std::endl;
    } catch (...) {
        CRITICAL("Unhandled exception");
    }
    CRITICAL("Failed to read note!");
    return boost::shared_ptr<Kolab::Note>();
}



template <>
boost::shared_ptr<Kolab::Configuration> deserializeObject <Kolab::Configuration> (const std::string& s, bool isUrl)
{
    clearErrors();
    try {
        std::auto_ptr<KolabXSD::Configuration> configuration;
        if (isUrl) {
            xsd::cxx::xml::dom::auto_ptr <xercesc::DOMDocument > doc = XMLParserWrapper::inst().parseFile(s);
            if (doc.get()) {
                configuration = KolabXSD::configuration(doc);
            }
        } else {
            xsd::cxx::xml::dom::auto_ptr <xercesc::DOMDocument > doc = XMLParserWrapper::inst().parseString(s);
            if (doc.get()) {
                configuration = KolabXSD::configuration(doc);
            }
        }

        if (!configuration.get()) {
            CRITICAL("failed to parse configuration!");
            return boost::shared_ptr<Kolab::Configuration>();
        }

        boost::shared_ptr<Kolab::Configuration> n;
        if (configuration->type() == KolabXSD::ConfigurationType::dictionary) {
            std::string lang("XX");
            if (configuration->language()) {
                lang = *configuration->language();
            } else {
                WARNING("missing dictionary language, default to special value XX");
            }
            Dictionary dict(lang);

            std::vector<std::string> entries;
            BOOST_FOREACH (const KolabXSD::Configuration::e_type &entry, configuration->e()) {
                entries.push_back(entry);
            }
            dict.setEntries(entries);
            
            n = boost::shared_ptr<Kolab::Configuration>(new Kolab::Configuration(dict));
        } else if (configuration->type() == KolabXSD::ConfigurationType::categorycolor) {
            n = boost::shared_ptr<Kolab::Configuration>(new Kolab::Configuration(readColors(configuration->categorycolor())));
        } else {
            CRITICAL("No valid configuration type");
        }

        n->setUid(configuration->uid());
        n->setCreated(*toDate(configuration->creation_date()));
        n->setLastModified(*toDate(configuration->last_modification_date()));

        setProductId( configuration->prodid() );
        //         setFormatVersion( vcards->vcard().version().text() );
        //         global_xCardVersion = vcalendar.properties().version().text();
        setKolabVersion( configuration->version() );
        return n;
    } catch  (const xml_schema::exception& e) {
        std::cerr <<  e << std::endl;
    } catch (...) {
        CRITICAL("Unhandled exception");
    }
    CRITICAL("Failed to read configuration!");
    return boost::shared_ptr<Kolab::Configuration>();
}

template <>
boost::shared_ptr<Kolab::File> deserializeObject <Kolab::File> (const std::string& s, bool isUrl)
{
    clearErrors();
    try {
        std::auto_ptr<KolabXSD::File> file;
        if (isUrl) {
            xsd::cxx::xml::dom::auto_ptr <xercesc::DOMDocument > doc = XMLParserWrapper::inst().parseFile(s);
            if (doc.get()) {
                file = KolabXSD::file(doc);
            }
        } else {
            xsd::cxx::xml::dom::auto_ptr <xercesc::DOMDocument > doc = XMLParserWrapper::inst().parseString(s);
            if (doc.get()) {
                file = KolabXSD::file(doc);
            }
        }

        if (!file.get()) {
            CRITICAL("failed to parse file!");
            return boost::shared_ptr<Kolab::File>();
        }

        boost::shared_ptr<Kolab::File> n = boost::shared_ptr<Kolab::File>(new Kolab::File);
        n->setUid(file->uid());
        n->setCreated(*toDate(file->creation_date()));
        n->setLastModified(*toDate(file->last_modification_date()));
        std::vector<std::string> categories;
        std::copy(file->categories().begin(), file->categories().end(), std::back_inserter(categories));
        n->setCategories(categories);
        if (file->classification()) {
            switch (*file->classification()) {
                case KolabXSD::File::classification_type::PUBLIC:
                    n->setClassification(Kolab::ClassPublic);
                    break;
                case KolabXSD::File::classification_type::PRIVATE:
                    n->setClassification(Kolab::ClassPrivate);
                    break;
                case KolabXSD::File::classification_type::CONFIDENTIAL:
                    n->setClassification(Kolab::ClassConfidential);
                    break;
                default:
                    ERROR("unknown classification");
            }
        }

        const Kolab::Attachment &attachment = toAttachment(file->file());
        if (attachment.label().empty()) {
            ERROR("Missing filename");
        }
        if (!attachment.isValid()) {
            ERROR("invalid attachment");
        }
        n->setFile(attachment);
        
        if (file->note()) {
            n->setNote(*file->note());
        }
        
        setProductId( file->prodid() );
        //         setFormatVersion( vcards->vcard().version().text() );
        //         global_xCardVersion = vcalendar.properties().version().text();
        setKolabVersion( file->version() );
        
        if (!file->x_custom().empty()) {
            std::vector<Kolab::CustomProperty> customProperties;
            BOOST_FOREACH(const KolabXSD::CustomType &p, file->x_custom()) {
                customProperties.push_back(CustomProperty(p.identifier(), p.value()));
            }
            n->setCustomProperties(customProperties);
        }
        return n;
    } catch  (const xml_schema::exception& e) {
        std::cerr <<  e << std::endl;
    } catch (...) {
        CRITICAL("Unhandled exception");
    }
    CRITICAL("Failed to read file!");
    return boost::shared_ptr<Kolab::File>();
}
    
    }//Namespace
} //Namespace

#endif
