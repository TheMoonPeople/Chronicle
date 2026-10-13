#include <cstdio>
#include <cstring>
#include <exception>

#include "dcdata.hpp"
#include "models.hpp"

namespace {

int Usage() {
    std::fputs("usage: dcdata extract <iso-or-directory> <data-directory>\n"
               "       dcdata list <iso-or-directory>\n"
               "       dcdata models <iso-or-directory> [--ref <pack>] [--detail <text>]...\n"
               "           lists every character model and how its skeleton compares with the\n"
               "           reference's (dun/mainchara/c01d.chr, Toan, unless named); with --detail,\n"
               "           only the details of the models whose place contains the text\n"
               "       dcdata seda <data-directory>\n"
               "           builds the Seda mod's pack into data extracted before (extract does it too)\n",
               stderr);
    return 2;
}

void Describe(const dcdata::Archive &archive) {
    dcdata::Log(stdout, "source: {} ({})\n", archive.source.string(),
                archive.image ? "ISO 9660 image" : "directory");
    dcdata::Log(stdout, "DATA.DAT: {} bytes, DATA.HD2: {} bytes\n", archive.dat.size, archive.hd2.size);
}

int List(const std::filesystem::path &source) {
    dcdata::Archive             archive = dcdata::OpenArchive(source);
    std::vector<unsigned char>  hd2 = dcdata::ReadExtent(archive.hd2);
    std::vector<dcdata::Record> records = dcdata::ParseIndex(hd2);
    std::uint64_t               bytes = 0;
    for (const dcdata::Record &record : records) {
        dcdata::Log(stdout, "{:8} {:10} {}\n", record.sector, record.size, record.path);
        bytes += record.size;
    }
    Describe(archive);
    dcdata::Log(stdout, "{} files, {} bytes\n", records.size(), bytes);
    return 0;
}

int Extract(const std::filesystem::path &source, const std::filesystem::path &out) {
    dcdata::Archive archive = dcdata::OpenArchive(source);
    Describe(archive);
    dcdata::Summary summary = dcdata::Extract(archive, out, stdout);

    std::vector<unsigned char>          hd2 = dcdata::ReadExtent(archive.hd2);
    std::vector<dcdata::Record>         records = dcdata::ParseIndex(hd2);
    std::vector<const dcdata::Record *> bad = dcdata::Mismatched(records, out);
    std::size_t                         reachable = summary.records - summary.duplicates;
    dcdata::Log(stdout, "index: {} records, {} files, {} bytes\n", summary.records, reachable, summary.bytes);
    dcdata::Log(stdout, "wrote {} files ({} bytes), kept {} already present\n", summary.written,
                summary.written_bytes, summary.kept);
    if (summary.written + summary.kept != reachable || !bad.empty()) {
        for (const dcdata::Record *record : bad) {
            dcdata::Log(stderr, "missing or wrong size: {}\n", record->path);
        }
        dcdata::Log(stderr, "verified {}/{} files in {}\n", reachable - bad.size(), reachable, out.string());
        return 1;
    }
    dcdata::Log(stdout, "verified {}/{} files in {}{}\n", reachable, reachable, out.string(),
                summary.warnings ? std::format(" ({} warnings)", summary.warnings) : "");
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 4 && std::strcmp(argv[1], "extract") == 0) {
            return Extract(argv[2], argv[3]);
        }
        if (argc == 3 && std::strcmp(argv[1], "list") == 0) {
            return List(argv[2]);
        }
        if (argc == 3 && std::strcmp(argv[1], "seda") == 0) {
            return dcdata::WriteSedaPack(argv[2], stdout) ? 0 : 1;
        }
        if (argc >= 3 && std::strcmp(argv[1], "models") == 0) {
            std::string              reference = "dun/mainchara/c01d.chr";
            std::vector<std::string> filters;
            for (int i = 3; i < argc; i++) {
                if (std::strcmp(argv[i], "--ref") == 0 && i + 1 < argc) {
                    reference = argv[++i];
                } else if (std::strcmp(argv[i], "--detail") == 0 && i + 1 < argc) {
                    filters.push_back(argv[++i]);
                } else if (argv[i][0] != '-' && i == 3) {
                    reference = argv[i]; // the older form: the reference pack alone
                } else {
                    return Usage();
                }
            }
            return dcdata::ModelReport(dcdata::OpenArchive(argv[2]), stdout, reference, filters);
        }
        return Usage();
    } catch (const std::exception &error) {
        std::fprintf(stderr, "dcdata: %s\n", error.what());
        return 1;
    }
}
