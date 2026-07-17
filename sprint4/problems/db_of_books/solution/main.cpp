#include <boost/json.hpp>
#include <iostream>
#include <pqxx/pqxx>
#include <string>

using namespace std::literals;
using pqxx::operator"" _zv;
namespace json = boost::json;

namespace {

constexpr auto tag_add_book = "add_book"_zv;

void EnsureSchema(pqxx::connection& conn) {
    pqxx::work w(conn);
    w.exec(
        "CREATE TABLE IF NOT EXISTS books "
        "(id SERIAL PRIMARY KEY, "
        "title varchar(100) NOT NULL, "
        "author varchar(100) NOT NULL, "
        "year integer NOT NULL, "
        "ISBN char(13) UNIQUE);"_zv);
    w.commit();
}

bool AddBook(pqxx::connection& conn, const json::object& payload) {
    try {
        pqxx::work w(conn);

        std::string title = json::value_to<std::string>(payload.at("title"));
        std::string author = json::value_to<std::string>(payload.at("author"));
        int year = static_cast<int>(payload.at("year").as_int64());

        const auto& isbn_val = payload.at("ISBN");

        if (isbn_val.is_null()) {
            w.exec_params(
                "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, NULL)"_zv,
                title, author, year);
        } else {
            std::string isbn = json::value_to<std::string>(isbn_val);
            w.exec_params(
                "INSERT INTO books (title, author, year, ISBN) VALUES ($1, $2, $3, $4)"_zv,
                title, author, year, isbn);
        }

        w.commit();
        return true;
    } catch (const pqxx::sql_error&) {
        return false;
    } catch (const std::exception&) {
        return false;
    }
}

json::array AllBooks(pqxx::connection& conn) {
    pqxx::read_transaction r(conn);

    json::array result;

    auto query_text =
        "SELECT id, title, author, year, ISBN FROM books "
        "ORDER BY year DESC, title ASC, author ASC, ISBN ASC;"_zv;

    for (auto [id, title, author, year, isbn] :
         r.query<int, std::string, std::string, int, std::optional<std::string>>(query_text)) {
        json::object book;
        book["id"] = id;
        book["title"] = title;
        book["author"] = author;
        book["year"] = year;
        if (isbn) {
            book["ISBN"] = *isbn;
        } else {
            book["ISBN"] = nullptr;
        }
        result.push_back(std::move(book));
    }

    return result;
}

}  // namespace

int main(int argc, const char* argv[]) {
    try {
        if (argc == 1) {
            std::cout << "Usage: book_manager <conn-string>\n"sv;
            return EXIT_SUCCESS;
        } else if (argc != 2) {
            std::cerr << "Invalid command line\n"sv;
            return EXIT_FAILURE;
        }

        pqxx::connection conn{argv[1]};
        EnsureSchema(conn);

        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) {
                continue;
            }

            json::value request = json::parse(line);
            const auto& obj = request.as_object();
            std::string action = json::value_to<std::string>(obj.at("action"));
            const auto& payload = obj.at("payload").as_object();

            if (action == "add_book") {
                bool success = AddBook(conn, payload);
                json::object response;
                response["result"] = success;
                std::cout << json::serialize(response) << std::endl;
            } else if (action == "all_books") {
                json::array books = AllBooks(conn);
                std::cout << json::serialize(books) << std::endl;
            } else if (action == "exit") {
                break;
            }
        }

    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
