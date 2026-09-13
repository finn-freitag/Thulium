#pragma once

#include <QString>
#include <QDateTime>

namespace pdn {

struct Metadata {
    QString title;
    QString author;
    QString copyright;
    QString description;
    QString creationDate;
    QString software = "Paint.NET Clone";

    bool isEmpty() const {
        return title.trimmed().isEmpty() &&
               author.trimmed().isEmpty() &&
               copyright.trimmed().isEmpty() &&
               description.trimmed().isEmpty() &&
               creationDate.trimmed().isEmpty();
    }

    void clear() {
        title.clear();
        author.clear();
        copyright.clear();
        description.clear();
        creationDate.clear();
        software.clear();
    }

    bool operator==(const Metadata& other) const {
        return title == other.title &&
               author == other.author &&
               copyright == other.copyright &&
               description == other.description &&
               creationDate == other.creationDate &&
               software == other.software;
    }

    bool operator!=(const Metadata& other) const {
        return !(*this == other);
    }
};

} // namespace pdn
