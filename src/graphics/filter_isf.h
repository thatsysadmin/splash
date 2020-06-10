/*
 * Copyright (C) 2020 Emmanuel Durand
 *
 * This file is part of Splash.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Splash is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Splash.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * @filter_isf.h
 * The FilterISF class
 */

#ifndef SPLASH_FILTER_ISF_H
#define SPLASH_FILTER_ISF_H

#include <filesystem>
#include <optional>

#include "./core/constants.h"
#include "./graphics/filter.h"
#include "./graphics/filter_custom.h"

namespace Splash
{

/*************/
class FilterISF : public Filter
{
  public:
    /**
     *  Constructor
     * \param root Root object
     */
    FilterISF(RootObject* root);

    /**
     * No copy constructor, but a move one
     */
    FilterISF(const FilterISF&) = delete;
    FilterISF(FilterISF&&) = default;
    FilterISF& operator=(const FilterISF&) = delete;

    /**
     *  Render the filter
     */
    void render() override;

  protected:
    /**
     * Extract the JSON header from an ISF source
     * \param source ISF source
     * \return Return the JSON text source
     */
    std::optional<std::string> getJSONHeader(const std::string& source);

  private:
    std::string _isfSourceFile{""};                         //!< User defined fragment shader filter source file
    bool _watchFile{false};                                 //!< If true, updates shader automatically if source file changes
    std::filesystem::file_time_type _lastSourceFileWrite{}; //!< Last time the shader source has been updated

    std::vector<std::shared_ptr<Texture_Image>> _passFilters{};

    /**
     * Register new functors to modify attributes
     */
    void registerAttributes() override;

    /**
     * Register attributes related to the default shader
     */
    void registerDefaultShaderAttributes() override{};

    /**
     * Set the filter ISF source file
     * \param source Source ISF file
     * \return Return true if the ISF file is valid and loaded successfully
     */
    bool setFilterSourceFile(const std::string& source);
};

} // namespace Splash

#endif // SPLASH_FILTER_ISF_H
