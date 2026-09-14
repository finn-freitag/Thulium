#include <QApplication>
#include <QFile>
#include <iostream>
#include <cassert>
#include "../src/core/Document.h"
#include "../src/io/PdnFormat.h"
#include "../src/io/StandardFormats.h"
#include "../src/ui/Dialogs.h"

int main(int argc, char* argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    std::cout << "=== Image Metadata Verification Test ===" << std::endl;

    // 1. Test Metadata struct operations
    {
        std::cout << "[Test 1] Metadata struct methods..." << std::endl;
        pdn::Metadata m;
        assert(m.isEmpty());

        m.title = "Sunset Over Hills";
        m.author = "Finn Developer";
        m.copyright = "Copyright (c) 2026";
        m.description = "A peaceful landscape created in Thulium.";
        m.creationDate = "2026-09-13T22:30:00";
        m.software = "Thulium";

        assert(!m.isEmpty());

        pdn::Metadata m2 = m;
        assert(m == m2);

        m2.title = "Different Title";
        assert(m != m2);

        m2.clear();
        assert(m2.isEmpty());
        std::cout << "  -> Passed!" << std::endl;
    }

    // 2. Test Document metadata setting, signals, and Undo/Redo
    {
        std::cout << "[Test 2] Document metadata Undo/Redo..." << std::endl;
        pdn::Document doc(200, 200);
        assert(!doc.isModified());

        pdn::Metadata initialMeta = doc.metadata();
        assert(initialMeta.isEmpty());

        pdn::Metadata newMeta;
        newMeta.title = "Masterpiece";
        newMeta.author = "Leonardo";
        newMeta.copyright = "Public Domain";
        newMeta.description = "Iconic artwork";
        newMeta.creationDate = "1503-01-01";
        newMeta.software = "Thulium";

        doc.setMetadata(newMeta, true);
        assert(doc.metadata() == newMeta);
        assert(doc.isModified());
        assert(doc.undoStack()->count() == 1);

        // Undo
        doc.undoStack()->undo();
        assert(doc.metadata() == initialMeta);
        assert(!doc.isModified());

        // Redo
        doc.undoStack()->redo();
        assert(doc.metadata() == newMeta);
        assert(doc.isModified());
        std::cout << "  -> Passed!" << std::endl;
    }

    // 3. Test PDN format roundtrip with metadata
    {
        std::cout << "[Test 3] PDN format metadata roundtrip..." << std::endl;
        pdn::Document doc(150, 100);
        pdn::Metadata meta;
        meta.title = "PDN Test Title";
        meta.author = "PDN Author";
        meta.copyright = "2026 Free Software";
        meta.description = "Testing PDN XML custom metadata preservation";
        meta.creationDate = "2026-09-13T22:00:00";
        meta.software = "Thulium v1.0";
        doc.setMetadata(meta, false);

        QString tmpPdn = "/tmp/test_metadata_roundtrip.pdn";
        QString err;
        bool saved = pdn::PdnFormat::save(doc, tmpPdn, &err);
        assert(saved);

        auto reloadedDoc = pdn::PdnFormat::load(tmpPdn, &err);
        assert(reloadedDoc != nullptr);
        const auto& loadedMeta = reloadedDoc->metadata();

        assert(loadedMeta.title == meta.title);
        assert(loadedMeta.author == meta.author);
        assert(loadedMeta.copyright == meta.copyright);
        assert(loadedMeta.description == meta.description);
        assert(loadedMeta.creationDate == meta.creationDate);
        assert(loadedMeta.software == meta.software);
        std::cout << "  -> Passed!" << std::endl;
    }

    // 4. Backward compatibility: loading existing Card.pdn without metadata
    {
        std::cout << "[Test 4] Card.pdn backward compatibility..." << std::endl;
        QString cardPath = (argc > 1) ? QString::fromUtf8(argv[1]) : "Card.pdn";
        if (!QFile::exists(cardPath) && QFile::exists("../Card.pdn")) {
            cardPath = "../Card.pdn";
        }
        QString err;
        auto cardDoc = pdn::PdnFormat::load(cardPath, &err);
        assert(cardDoc != nullptr);
        assert(cardDoc->width() == 870 && cardDoc->height() == 560);
        assert(cardDoc->layerCount() == 5);
        // Should have empty metadata without errors
        assert(cardDoc->metadata().title.isEmpty());
        std::cout << "  -> Passed!" << std::endl;
    }

    // 5. Test PNG export and import metadata
    {
        std::cout << "[Test 5] PNG format metadata embedding & extraction..." << std::endl;
        pdn::Document doc(64, 64);
        pdn::Metadata meta;
        meta.title = "PNG Image Title";
        meta.author = "PNG Artist";
        meta.copyright = "Copyright 2026 PNG";
        meta.description = "A description in PNG tEXt chunk";
        meta.creationDate = "2026-09-13T22:15:00";
        meta.software = "Thulium";
        doc.setMetadata(meta, false);

        QString tmpPng = "/tmp/test_metadata.png";
        QString err;
        bool saved = pdn::StandardFormats::save(doc, tmpPng, "png", 100, &err);
        assert(saved);

        auto loadedPng = pdn::StandardFormats::load(tmpPng, &err);
        assert(loadedPng != nullptr);
        const auto& pngMeta = loadedPng->metadata();

        assert(pngMeta.title == meta.title);
        assert(pngMeta.author == meta.author);
        assert(pngMeta.description == meta.description);
        assert(pngMeta.copyright == meta.copyright);
        std::cout << "  -> Passed!" << std::endl;
    }

    // 6. Test JPEG export and import metadata (EXIF APP1)
    {
        std::cout << "[Test 6] JPEG format metadata embedding & extraction (EXIF)..." << std::endl;
        pdn::Document doc(64, 64);
        pdn::Metadata meta;
        meta.title = "JPEG Image Title";
        meta.author = "JPEG Photographer";
        meta.copyright = "Copyright 2026 JPEG";
        meta.description = "A photo description";
        meta.creationDate = "2026-09-13T22:15:00";
        meta.software = "Thulium";
        doc.setMetadata(meta, false);

        QString tmpJpg = "/tmp/test_metadata.jpg";
        QString err;
        bool saved = pdn::StandardFormats::save(doc, tmpJpg, "jpg", 95, &err);
        assert(saved);

        auto loadedJpg = pdn::StandardFormats::load(tmpJpg, &err);
        assert(loadedJpg != nullptr);
        const auto& jpgMeta = loadedJpg->metadata();

        assert(jpgMeta.title == meta.title);
        assert(jpgMeta.author == meta.author);
        assert(jpgMeta.description == meta.description);
        assert(jpgMeta.copyright == meta.copyright);
        std::cout << "  -> Passed!" << std::endl;
    }

    // 7. Test MetadataDialog interaction (Clear All, untouched metadata preservation)
    {
        std::cout << "[Test 7] MetadataDialog interaction & Clear All..." << std::endl;
        pdn::Document doc(100, 100);
        pdn::Metadata meta;
        meta.title = "Sample Title";
        meta.author = "Sample Author";
        meta.copyright = "Sample Copyright";
        meta.description = "Sample Description";
        meta.creationDate = "2026-09-13T22:30:00";
        meta.software = "Thulium";
        doc.setMetadata(meta, false);

        pdn::MetadataDialog dlg(&doc);
        // If user doesn't touch anything:
        assert(dlg.metadata() == meta);

        // Click Clear All
        dlg.onClearAll();
        assert(dlg.metadata().isEmpty());

        // Set date to Now
        dlg.onSetCurrentDateTime();
        assert(!dlg.metadata().creationDate.isEmpty());
        std::cout << "  -> Passed!" << std::endl;
    }

    std::cout << "\nALL METADATA TESTS PASSED SUCCESSFULLY!" << std::endl;
    return 0;
}
