#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../domain/author_fwd.h"
#include "../domain/book_fwd.h"
#include "../domain/book.h"

namespace app {

struct AuthorInfo {
    std::string id;
    std::string name;
};

struct BookInfo {
    std::string id;
    std::string author_id;
    std::string author_name;
    std::string title;
    int publication_year;
};

struct BookDetails {
    std::string title;
    std::string author_name;
    int publication_year;
    std::vector<std::string> tags;
};

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual std::vector<AuthorInfo> GetAuthors() = 0;
    virtual std::optional<AuthorInfo> GetAuthorByName(const std::string& name) = 0;

    virtual void AddBook(const std::string& author_id, const std::string& title,
                          int publication_year, const std::vector<std::string>& tags) = 0;
    virtual std::vector<BookInfo> GetBooks() = 0;
    virtual std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) = 0;
    virtual std::vector<BookInfo> GetBooksByTitle(const std::string& title) = 0;
    virtual std::optional<BookDetails> GetBookDetails(const std::string& book_id) = 0;

    virtual bool DeleteAuthor(const std::string& author_id) = 0;
    virtual bool DeleteAuthorByName(const std::string& name) = 0;
    virtual bool EditAuthor(const std::string& author_id, const std::string& new_name) = 0;

    virtual bool DeleteBook(const std::string& book_id) = 0;
    virtual bool EditBook(const std::string& book_id, const std::string& new_title,
                          int new_year, const std::vector<std::string>& new_tags) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
