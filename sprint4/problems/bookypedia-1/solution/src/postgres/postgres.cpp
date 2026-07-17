#include <pqxx/pqxx>
#include "postgres.h"

#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAuthors() {
    pqxx::read_transaction r{connection_};
    std::vector<domain::Author> authors;

    auto result = r.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    for (const auto& row : result) {
        authors.emplace_back(domain::AuthorId::FromString(row["id"].as<std::string>()),
                              row["name"].as<std::string>());
    }
    return authors;
}

void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(),
        book.GetPublicationYear());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAllBooks() {
    pqxx::read_transaction r{connection_};
    std::vector<domain::Book> books;

    auto result = r.exec(
        "SELECT id, author_id, title, publication_year FROM books ORDER BY title;"_zv);
    for (const auto& row : result) {
        books.emplace_back(domain::BookId::FromString(row["id"].as<std::string>()),
                            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
                            row["title"].as<std::string>(),
                            row["publication_year"].as<int>());
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetAuthorBooks(const domain::AuthorId& author_id) {
    pqxx::read_transaction r{connection_};
    std::vector<domain::Book> books;

    auto result = r.exec_params(
        "SELECT id, title, publication_year FROM books "
        "WHERE author_id=$1 ORDER BY publication_year, title;"_zv,
        author_id.ToString());
    for (const auto& row : result) {
        books.emplace_back(domain::BookId::FromString(row["id"].as<std::string>()), author_id,
                            row["title"].as<std::string>(), row["publication_year"].as<int>());
    }
    return books;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL,
    title varchar(100) NOT NULL,
    publication_year integer
);
)"_zv);
    work.commit();
}

}  // namespace postgres
