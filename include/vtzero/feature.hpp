#ifndef VTZERO_FEATURE_HPP
#define VTZERO_FEATURE_HPP

/*****************************************************************************

vtzero - Minimalistic vector tile decoder and encoder in C++.

This file is from https://github.com/mapbox/vtzero where you can find more
documentation.

*****************************************************************************/

/**
 * @file feature.hpp
 *
 * @brief Contains the feature class.
 */

#include "exception.hpp"
#include "property.hpp"
#include "property_value.hpp"
#include "types.hpp"

#include <protozero/pbf_message.hpp>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <utility>

namespace vtzero {

    /**
     * A feature according to spec 4.2.
     */
    class feature {

        uint64_t m_id = 0; // defaults to 0, see https://github.com/mapbox/vector-tile-spec/blob/master/2.1/vector_tile.proto#L32
        property_map m_properties {};
        data_view m_geometry{};
        data_view m_knots{};
        GeomType m_geometry_type = GeomType::UNKNOWN; // defaults to UNKNOWN, see https://github.com/mapbox/vector-tile-spec/blob/master/2.1/vector_tile.proto#L41
        bool m_has_id = false;

    public:

        /**
         * Construct an invalid feature object.
         */
        feature() = default;

        /**
         * Construct a feature object.
         *
         * @throws format_exception if the layer data is ill-formed.
         */
        feature(const layer* layer, const data_view data) :
            m_properties(layer) {
            vtzero_assert(layer);
            vtzero_assert(data.data());

            protozero::pbf_message<detail::pbf_feature> reader{data};

            while (reader.next()) {
                switch (reader.tag_and_type()) {
                    case protozero::tag_and_type(detail::pbf_feature::id, protozero::pbf_wire_type::varint):
                        m_id = reader.get_uint64();
                        m_has_id = true;
                        break;
                    case protozero::tag_and_type(detail::pbf_feature::tags, protozero::pbf_wire_type::length_delimited):
                        m_properties.initialize(reader.get_packed_uint32());
                        break;
                    case protozero::tag_and_type(detail::pbf_feature::type, protozero::pbf_wire_type::varint): {
                            const auto type = reader.get_enum();
                            // spec 4.3.4 "Geometry Types"
                            if (type < 0 || type > 3) {
                                throw format_exception{"Unknown geometry type (spec 4.3.4)"};
                            }
                            m_geometry_type = static_cast<GeomType>(type);
                        }
                        break;
                    case protozero::tag_and_type(detail::pbf_feature::geometry, protozero::pbf_wire_type::length_delimited):
                        if (!m_geometry.empty()) {
                            throw format_exception{"Feature has more than one geometry field"};
                        }
                        m_geometry = reader.get_view();
                        break;
                    case protozero::tag_and_type(detail::pbf_feature::knots, protozero::pbf_wire_type::length_delimited):
                        if (!m_knots.empty()) {
                            throw format_exception{"Feature has more than one knots field"};
                        }
                        m_knots = reader.get_view();
                        break;
                    default:
                        reader.skip(); // ignore unknown fields
                }
            }

            // spec 4.2 "A feature MUST contain a geometry field."
            if (m_geometry.empty()) {
                throw format_exception{"Missing geometry field in feature (spec 4.2)"};
            }
        }

        /**
         * Is this a valid feature? Valid features are those not created from
         * the default constructor.
         *
         * Complexity: Constant.
         */
        bool valid() const noexcept {
            return m_geometry.data() != nullptr;
        }

        /**
         * Is this a valid feature? Valid features are those not created from
         * the default constructor.
         *
         * Complexity: Constant.
         */
        explicit operator bool() const noexcept {
            return valid();
        }

        /**
         * The ID of this feature. According to the spec IDs should be unique
         * in a layer if they are set (spec 4.2).
         *
         * Complexity: Constant.
         *
         * Always returns 0 for invalid features.
         */
        uint64_t id() const noexcept {
            return m_id;
        }

        /**
         * Does this feature have an ID?
         *
         * Complexity: Constant.
         *
         * Always returns false for invalid features.
         */
        bool has_id() const noexcept {
            return m_has_id;
        }

        /**
         * The geometry type of this feature.
         *
         * Complexity: Constant.
         *
         * Always returns GeomType::UNKNOWN for invalid features.
         */
        GeomType geometry_type() const noexcept {
            return m_geometry_type;
        }

        /**
         * Get the geometry of this feature.
         *
         * Complexity: Constant.
         *
         * @pre @code valid() @endcode
         */
        vtzero::geometry geometry() const noexcept {
            vtzero_assert_in_noexcept_function(valid());
            return {m_geometry, m_knots, m_geometry_type};
        }

        /**
         * Returns true if this feature doesn't have any properties.
         *
         * Complexity: Constant.
         *
         * Always returns true for invalid features.
         */
        bool empty() const noexcept {
            return m_properties.empty();
        }

        /**
         * Returns the number of properties in this feature.
         *
         * Complexity: Constant.
         *
         * Always returns 0 for invalid features.
         */
        std::size_t num_properties() const noexcept {
            return m_properties.size();
        }

        /**
         * Get the next property in this feature.
         *
         * Complexity: Constant.
         *
         * @returns The next property or the invalid property if there are no
         *          more properties.
         * @throws format_exception if the feature data is ill-formed.
         * @throws any protozero exception if the protobuf encoding is invalid.
         * @pre @code valid() @endcode
         */
        property next_property() {
            return m_properties.next();
        }

        /**
         * Get the indexes into the key/value table for the next property in
         * this feature.
         *
         * Complexity: Constant.
         *
         * @returns The next index_value_pair or an invalid index_value_pair
         *          if there are no more properties.
         * @throws format_exception if the feature data is ill-formed.
         * @throws any protozero exception if the protobuf encoding is invalid.
         * @pre @code valid() @endcode
         */
        index_value_pair next_property_indexes() {
            return m_properties.next_indexes();
        }

        /**
         * Reset the property iterator. The next time next_property() or
         * next_property_indexes() is called, it will begin from the first
         * property again.
         *
         * Complexity: Constant.
         *
         * @pre @code valid() @endcode
         */
        void reset_property() noexcept {
            vtzero_assert_in_noexcept_function(valid());
            m_properties.reset();
        }

        /**
         * Call a function for each property of this feature.
         *
         * @tparam TFunc The type of the function. It must take a single
         *         argument of type property&& and return a bool. If the
         *         function returns false, the iteration will be stopped.
         * @param func The function to call.
         * @returns true if the iteration was completed and false otherwise.
         * @pre @code valid() @endcode
         */
        template <typename TFunc>
        bool for_each_property(TFunc&& func) const {
            return m_properties.for_each_property(func);
        }

    }; // class feature

    /**
     * Create some kind of mapping from property keys to property values.
     *
     * This can be used to read all properties into a std::map or similar
     * object.
     *
     * @tparam TMap Map type (std::map, std::unordered_map, ...) Must support
     *              the emplace() method.
     * @tparam TKey Key type, usually the key of the map type. The data_view
     *              of the property key is converted to this type before
     *              adding it to the map.
     * @tparam TValue Value type, usally the value of the map type. The
     *                property_value is converted to this type before
     *                adding it to the map.
     * @param feature The feature to get the properties from.
     * @returns An object of type TMap with all the properties.
     * @pre @code feature.valid() @endcode
     */
    template <typename TVariant,
              typename TMapping = property_value_mapping,
              typename TMap = typename TMapping::template map_type<typename TMapping::string_type, TVariant>>
    TMap create_properties_map(const vtzero::feature& feature) {
        using TKey = typename TMap::key_type;
        TMap map;

        feature.for_each_property([&](property&& p) {
            map.emplace(TKey(p.key()), convert_property_value<TVariant, TMapping>(p.value()));
            return true;
        });

        return map;
    }

} // namespace vtzero

#endif // VTZERO_FEATURE_HPP
