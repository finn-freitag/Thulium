#include "ParametricEffect.h"

namespace pdn {

namespace {

// Slider dialog generated from the parameter list.
class ParametricDialog : public EffectDialog {
public:
    ParametricDialog(Document* doc, const QString& name, const QVector<EffectParam>& params,
                     const EffectParamValues& initial, ParametricFn fn, QWidget* parent)
        : EffectDialog(doc, name, parent), m_values(initial), m_fn(std::move(fn)) {
        for (int i = 0; i < params.size(); ++i) {
            const EffectParam& p = params[i];
            // The slider starts at the last used value but "Reset" returns to the default.
            SliderControls controls = addSlider(p.label + ":", p.minimum, p.maximum, p.defaultValue,
                                                [this, i](int value) { m_values[i] = value; });
            controls.slider->setValue(initial[i]);
        }
        setupButtons();
        updatePreview();
    }

    const EffectParamValues& values() const { return m_values; }

    EffectApplier applier() const override {
        return [values = m_values, fn = m_fn](QImage& image, const Selection& selection) {
            fn(image, selection, values);
        };
    }

protected:
    void processPreview(QImage& image) override { m_fn(image, m_doc->selection(), m_values); }

private:
    EffectParamValues m_values;
    ParametricFn m_fn;
};

} // namespace

ParametricEffect::ParametricEffect(QString name, QString category, QVector<EffectParam> params, ParametricFn fn)
    : m_name(std::move(name)), m_category(std::move(category)), m_params(std::move(params)), m_fn(std::move(fn)) {
    for (const EffectParam& p : m_params) {
        m_values.append(p.defaultValue);
    }
}

bool ParametricEffect::apply(QImage& image, const Selection& selection) {
    m_fn(image, selection, m_values);
    return true;
}

bool ParametricEffect::showDialog(QWidget* parent, Document* doc) {
    if (!doc || !doc->activeLayer()) return false;
    if (m_params.isEmpty()) {
        return applyInstantEffect(doc, m_name, [this](QImage& image, const Selection& selection) {
            m_fn(image, selection, m_values);
        });
    }
    ParametricDialog dialog(doc, m_name, m_params, m_values, m_fn, parent);
    if (!runDialog(dialog)) return false;
    m_values = dialog.values();
    return true;
}

} // namespace pdn
