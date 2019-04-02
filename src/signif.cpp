#include "signif.h"

void signif(Parameters& parameters) {

    /* The significant_sequences function parses through a file generated by process_reads and outputs sequences significantly associated with sex.
     * Association with sex is determined using a Chi-squared test, and p-value is corrected with Bonferroni correction.
     * - Found in M males with min_males <= M <= max_males
     * - Found in F females with min_females <= F <= max_females
     */

    std::unordered_map<std::string, bool> popmap = load_popmap(parameters);

    uint total_males = 0, total_females = 0;
    for (auto i: popmap) if (i.second) ++total_males; else ++total_females;

    std::ifstream input_file;
    input_file.open(parameters.markers_table_path);

    if (input_file) {

        std::ofstream output_file;
        output_file.open(parameters.output_file_path);

        // First line is the header. The header is parsed to get the sex of each field in the table.
        std::vector<std::string> line;
        std::string temp = "";
        std::getline(input_file, temp);
        if (not parameters.output_fasta) output_file << temp << "\n"; // Copy the header line to the subset output file
        line = split(temp, "\t");

        // Map with column number --> index of sex_count (0 = male, 1 = female, 2 = no sex)
        std::unordered_map<uint, uint> sex_columns = get_column_sex(popmap, line);

        // Define variables used to read the file
        char buffer[65536];
        std::string temp_line;
        uint k = 0, field_n = 0, seq_count = 0;
        uint sex_count[3] = {0, 0, 0}; // Index: 0 = male, 1 = female, 2 = no sex information
        double chi_squared = 0, p = 0;

        std::map<std::string, double[3]> candidate_sequences;

        do {

            // Read a chunk of size given by the buffer
            input_file.read(buffer, sizeof(buffer));
            k = static_cast<uint>(input_file.gcount());

            for (uint i=0; i<k; ++i) {

                // Read the buffer character by character
                switch(buffer[i]) {

                    case '\t':  // New field
                        if (sex_columns[field_n] != 2 and static_cast<uint>(std::stoi(temp)) >= parameters.min_depth) ++sex_count[sex_columns[field_n]];  // Increment the appropriate counter
                        temp = "";
                        temp_line += buffer[i];
                        ++field_n;
                        break;

                    case '\n':  // New line (also a new field)
                        if (sex_columns[field_n] != 2 and static_cast<uint>(std::stoi(temp)) >= parameters.min_depth) ++sex_count[sex_columns[field_n]];  // Increment the appropriate counter
                        if (sex_count[0] + sex_count[1] > 0) {
                            ++seq_count;
                            chi_squared = get_chi_squared(sex_count[0], sex_count[1], total_males, total_females);
                            p = get_chi_squared_p(chi_squared);
                            if (static_cast<float>(p) < parameters.signif_threshold) { // First pass: we filter sequences with at least one male or one female and non-corrected p < 0.05
                                candidate_sequences[temp_line][0] = p;
                                candidate_sequences[temp_line][1] = sex_count[0];
                                candidate_sequences[temp_line][2] = sex_count[1];
                            }
                        }
                        // Reset variables
                        temp = "";
                        temp_line = "";
                        field_n = 0;
                        sex_count[0] = 0;
                        sex_count[1] = 0;
                        break;

                    default:
                        temp += buffer[i];
                        temp_line += buffer[i];
                        break;
                }
            }

        } while(input_file);

        if (not parameters.disable_correction) parameters.signif_threshold /= seq_count; // Bonferroni correction: divide threshold by number of tests

        // Second pass: filter with bonferroni
        for (auto sequence: candidate_sequences) {
            if (static_cast<float>(sequence.second[0]) < parameters.signif_threshold) {
                if (parameters.output_fasta) {
                    line = split(sequence.first, "\t");
                    output_file << ">" << line[0] << "_" << int(sequence.second[1]) << "M_" << int(sequence.second[2]) << "F_cov:" << parameters.min_depth << "_p:" << sequence.second[0] << "\n" << line[1] << "\n";
                } else {
                    output_file << sequence.first << "\n";
                }
            }
        }

        output_file.close();
        input_file.close();
    }
}
