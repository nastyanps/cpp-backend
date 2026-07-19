#pragma once
#include "../domain/unit_of_work.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    explicit UseCasesImpl(domain::UnitOfWorkFactory& uow_factory)
        : uow_factory_{uow_factory} {
    }

    void AddAuthor(const std::string& name) override;
    std::vector<AuthorInfo> GetAuthors() override;
    std::optional<AuthorInfo> GetAuthorByName(const std::string& name) override;

    void AddBook(const std::string& author_id, const std::string& title, int publication_year,
                 const std::vector<std::string>& tags) override;
    std::vector<BookInfo> GetBooks() override;
    std::vector<BookInfo> GetAuthorBooks(const std::string& author_id) override;
    std::vector<BookInfo> GetBooksByTitle(const std::string& title) override;
    std::optional<BookDetails> GetBookDetails(const std::string& book_id) override;

    bool DeleteAuthor(const std::string& author_id) override;
    bool DeleteAuthorByName(const std::string& name) override;
    bool EditAuthor(const std::string& author_id, const std::string& new_name) override;

    bool DeleteBook(const std::string& book_id) override;
    bool EditBook(const std::string& book_id, const std::string& new_title, int new_year,
                  const std::vector<std::string>& new_tags) override;

private:
    domain::UnitOfWorkFactory& uow_factory_;

    static std::vector<std::string> NormalizeTags(const std::vector<std::string>& raw_tags);
};

}  // namespace app
