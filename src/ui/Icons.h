#pragma once
#include <QColor>
#include <QIcon>
#include <QRectF>

class QPainter;

// Iconos vectoriales dibujados a mano (sin dependencia de Qt SVG) para
// reproducir el trazo fino del skin de referencia a cualquier DPI.
namespace Icons {

enum Icon {
    None = 0,
    Play, Pause, Stop, Prev, Next,
    Shuffle, Repeat, RepeatOne, ABLoop,
    Volume, VolumeLow, VolumeMute,
    Equalizer, Clock, Star, StarOutline,
    Plus, Minus, More, SortUpDown, Search, Menu, Close,
    Folder, FolderOpen, ChevronRight, ChevronDown, ChevronUp, Check,
    WinMinimize, WinMaximize, WinRestore, WinClose,
    Grip, Tag, Save, Revert, Image, Info, Trash, Refresh, ArrowRight, Settings
};

// Dibuja `id` centrado en `rect`, en el color dado.
void paint(QPainter& p, Icon id, const QRectF& rect,
           const QColor& color, qreal strokeWidth = 1.6);

// Version cacheada, para modelos y vistas.
QIcon icon(Icon id, const QColor& color, int px = 16);

} // namespace Icons
