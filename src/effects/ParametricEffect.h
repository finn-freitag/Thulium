#pragma once

#include "EffectDialog.h"
#include "IEffect.h"
#include <QVector>
#include <functional>

namespace pdn {

// One integer slider of a parametric effect.
struct EffectParam {
    QString label;
    int minimum = 0;
    int maximum = 100;
    int defaultValue = 0;
};

using EffectParamValues = QVector<int>;
// Applies the effect to image (Format_ARGB32) inside selection with one value per EffectParam.
using ParametricFn = std::function<void(QImage& image, const Selection& selection, const EffectParamValues& values)>;

// An effect described by data: a name, a list of slider parameters and a processing function.
// The settings dialog, live preview, undo and "Repeat Effect" support all come for free, so
// adding an effect means writing only its pixel loop (see MoreEffects.cpp).
class ParametricEffect : public IEffect {
public:
    ParametricEffect(QString name, QString category, QVector<EffectParam> params, ParametricFn fn);

    QString name() const override { return m_name; }
    QString category() const override { return m_category; }

    bool apply(QImage& image, const Selection& selection) override;
    bool showDialog(QWidget* parent, Document* doc) override;

private:
    QString m_name;
    QString m_category;
    QVector<EffectParam> m_params;
    ParametricFn m_fn;
    EffectParamValues m_values; // last confirmed settings; the dialog reopens with them
};

// Effects built on ParametricEffect, in menu order.
QList<std::shared_ptr<IEffect>> createParametricEffects();

} // namespace pdn
