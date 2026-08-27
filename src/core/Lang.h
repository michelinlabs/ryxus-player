#pragma once

#include <QString>
#include <QVector>

// Traduccion de la interfaz.
//
// El idioma de origen es el espanol: las cadenas del codigo son la clave, y
// aqui se buscan en la tabla del idioma activo. Es mas simple que arrastrar
// .ts/.qm por dos idiomas, y evita tener que reconstruir para tocar un texto.
namespace Lang {

enum class Id {
    Spanish,
    English
};

struct Entry {
    Id      id;
    QString code;        // "es" / "en"
    QString nativeName;  // "Espanol" / "English"
};

const QVector<Entry>& available();

Id      current();
void    setCurrent(Id id);
QString code(Id id);
Id      fromCode(const QString& code);

// Devuelve `spanish` traducido al idioma activo. Si no hay entrada en la
// tabla, devuelve el original: un texto sin traducir se ve, pero no rompe.
QString tr(const char* spanish);

} // namespace Lang
