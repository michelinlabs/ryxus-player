#pragma once

// Escala automatica de la interfaz.
//
// El diseno esta hecho a medidas fijas en pixeles, asi que en pantallas mas
// pequenas que la de referencia no cabe entero. En vez de rehacer el reparto,
// se encoge la interfaz completa -- tipografia incluida -- hasta que cabe.
namespace UiScale {

// Ajusta la escala global al tamano de la pantalla.
//
// Hay que llamarla ANTES de construir QApplication: Qt lee el factor una sola
// vez, al inicializar la GUI, y despues ya no se puede cambiar.
void applyForCurrentScreen();

} // namespace UiScale
