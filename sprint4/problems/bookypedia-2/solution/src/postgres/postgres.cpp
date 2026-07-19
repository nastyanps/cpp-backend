#include <pqxx/pqxx>
#include "postgres.h"

#include <boost/algorithm/string/join.hpp>
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

// ------------------- AuthorRepositoryImpl -------------------

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    work_.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAuthors() {
    std::vector<domain::Author> authors;
    auto result = work_.exec("SELECT id, name FROM authors ORDER BY name;"_zv);
    for (const auto& row : result) {
        authors.emplace_back(domain::AuthorId::FromString(row["id"].as<std::string>()),
                              row["name"].as<std::string>());
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetAuthorByName(const std::string& name) {
    auto result = work_.exec_params(
        "SELECT id, name FROM authors WHERE name=$1;"_zv, name);
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result[0];
    return domain::Author{domain::AuthorId::FromString(row["id"].as<std::string>()),
                           row["name"].as<std::string>()};
}

std::optional<domain::Author> AuthorRepositoryImpl::GetAuthorById(const domain::AuthorId& id) {
    auto result = work_.exec_params(
        "SELECT id, name FROM authors WHERE id=$1;"_zv, id.ToString());
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result[0];
    return domain::Author{domain::AuthorId::FromString(row["id"].as<std::string>()),
                           row["name"].as<std::string>()};
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    work_.exec_params("DELETE FROM authors WHERE id=$1;"_zv, id.ToString());
}

void AuthorRepositoryImpl::Edit(const domain::AuthorId& id, const std::string& new_name) {
    auto result = work_.exec_params(
        "UPDATE authors SET name=$2 WHERE id=$1;"_zv, id.ToString(), new_name);
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Author not found");
    }
}

// ------------------- BookRepositoryImpl -------------------

void BookRepositoryImpl::Save(const domain::Book& book) {
    work_.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(),
        book.GetPublicationYear());
    SaveTags(book.GetId(), book.GetTags());
}

std::vector<std::string> BookRepositoryImpl::GetTags(const domain::BookId& book_id) {
    std::vector<std::string> tags;
    auto result = work_.exec_params(
        "SELECT tag FROM book_tags WHERE book_id=$1 ORDER BY tag;"_zv, book_id.ToString());
    for (const auto& row : result) {
        tags.push_back(row["tag"].as<std::string>());
    }
    return tags;
}

void BookRepositoryImpl::SaveTags(const domain::BookId& book_id,
                                   const std::vector<std::string>& tags) {
    for (const auto& tag : tags) {
        work_.exec_params(
            "INSERT INTO book_tags (book_id, tag) VALUES ($1, $2);"_zv, book_id.ToString(), tag);
    }
}

std::vector<domain::Book> BookRepositoryImpl::GetAllBooks() {
    std::vector<domain::Book> books;
    auto result = work_.exec(
        "SELECT id, author_id, title, publication_year FROM books ORDER BY title;"_zv);
    for (const auto& row : result) {
        domain::BookId id = domain::BookId::FromString(row["id"].as<std::string>());
        books.emplace_back(id, domain::AuthorId::FromString(row["author_id"].as<std::string>()),
                            row["title"].as<std::string>(), row["publication_year"].as<int>(),
                            GetTags(id));
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetAuthorBooks(const domain::AuthorId& author_id) {
    std::vector<domain::Book> books;
    auto result = work_.exec_params(
        "SELECT id, title, publication_year FROM books "
        "WHERE author_id=$1 ORDER BY publication_year, title;"_zv,
        author_id.ToString());
    for (const auto& row : result) {
        domain::BookId id = domain::BookId::FromString(row["id"].as<std::string>());
        books.emplace_back(id, author_id, row["title"].as<std::string>(),
                            row["publication_year"].as<int>(), GetTags(id));
    }
    return books;
}

std::vector<domain::BookInfo> BookRepositoryImpl::GetAllBooksWithAuthors() {
    std::vector<domain::BookInfo> books;
    auto result = work_.exec(
        R"(
SELECT b.id, b.author_id, a.name AS author_name, b.title, b.publication_year
FROM books b JOIN authors a ON b.author_id = a.id
ORDER BY b.title, a.name, b.publication_year;
)"_zv);
    for (const auto& row : result) {
        books.push_back(domain::BookInfo{
            domain::BookId::FromString(row["id"].as<std::string>()),
            domain::AuthorId::FromString(row["author_id"].as<std::string>()),
            row["author_name"].as<std::string>(),
            row["title"].as<std::string>(),
            row["publication_year"].as<int>()});
    }
    return books;
}

std::optional<domain::Book> BookRepositoryImpl::GetBookById(const domain::BookId& id) {
    auto result = work_.exec_params(
        "SELECT id, author_id, title, publication_year FROM books WHERE id=$1;"_zv,
        id.ToString());
    if (result.empty()) {
        return std::nullopt;
    }
    const auto& row = result[0];
    return domain::Book{id, domain::AuthorId::FromString(row["author_id"].as<std::string>()),
                         row["title"].as<std::string>(), row["publication_year"].as<int>(),
                         GetTags(id)};
}

void BookRepositoryImpl::Delete(const domain::BookId& id) {
    work_.exec_params("DELETE FROM book_tags WHERE book_id=$1;"_zv, id.ToString());
    work_.exec_params("DELETE FROM books WHERE id=$1;"_zv, id.ToString());
}

void BookRepositoryImpl::Edit(const domain::BookId& id, const std::string& new_title,
                               int new_year, const std::vector<std::string>& new_tags) {
    auto result = work_.exec_params(
        "UPDATE books SET title=$2, publication_year=$3 WHERE id=$1;"_zv, id.ToString(),
        new_title, new_year);
    if (result.affected_rows() == 0) {
        throw std::runtime_error("Book not found");
    }
    work_.exec_params("DELETE FROM book_tags WHERE book_id=$1;"_zv, id.ToString());
    SaveTags(id, new_tags);
}

void BookRepositoryImpl::DeleteByAuthor(const domain::AuthorId& author_id) {
    work_.exec_params(
        "DELETE FROM book_tags WHERE book_id IN (SELECT id FROM books WHERE author_id=$1);"_zv,
        author_id.ToString());
    work_.exec_params("DELETE FROM books WHERE author_id=$1;"_zv, author_id.ToString());
}

// ------------------- Database -------------------

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
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL,
    tag varchar(30) NOT NULL
);
)"_zv);
    work.commit();
}

}  // namespace postgres
